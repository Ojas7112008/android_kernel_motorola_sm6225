#!/usr/bin/env python3
"""Live FUSE index/lifecycle and native directory parsers under explicit host models.

No native kernel dcache/RCU/FUSE daemon, Kbuild/module link, or device is executed.
"""
import json
from pathlib import Path
import re
from kernel_baseline import kernel_source
import subprocess
import sys
sys.dont_write_bytecode = True
from test_selinux_adapter import function, run, digest
from test_path_consumers import prepare
from test_setresuid_task_state import section


def main():
    root = Path(__file__).resolve().parents[2]
    out = root/".backport-context/audit/fuse-path-index"
    prepare(root, out)
    source = (root/"fs/fuse/inode.c").read_text()
    code = function((root/"fs/fuse/fuse_i.h").read_text(), "get_fuse_inode")
    for name in ["fuse_fill_attr_from_inode", "fuse_alloc_inode", "fuse_free_inode", "fuse_evict_inode", "fuse_destroy_inode", "fuse_squash_ino", "fuse_change_attributes_common"]:
        body = section(source,"static ino_t fuse_squash_ino(") if name=="fuse_squash_ino" else function(source, name)
        if name=="fuse_destroy_inode": body = "#ifdef CONFIG_FUSE_BPF\n"+body+"#endif\n"
        code += body
    (out/"fuse_lifecycle.h").write_text(code)
    source = (root/"fs/fuse/readdir.c").read_text()
    code = function((root/"include/linux/fs.h").read_text(), "dir_emit")
    code += function(source, "fuse_emit") + function(source, "parse_dirfile") + function(source, "parse_dirplusfile")
    code += re.search(r"enum fuse_parse_result \{.*?\n\};",source,re.S).group(0)+"\n"
    code += section(source, "static enum fuse_parse_result fuse_parse_cache(")
    backing=(root/"fs/fuse/backing.c").read_text()
    code += function(backing, "parse_dirfile").replace("parse_dirfile(","parse_backing_dirfile(",1)
    (out/"fuse_parsers.h").write_text(code)
    # Guard removal must retain every native allocation, attribute and free path exactly.
    from test_path_consumers import without_hooks
    for file in ["fs/fuse/inode.c","fs/fuse/fuse_i.h"]:
        assert without_hooks((root/file).read_text())==kernel_source(root, file),file
    assert re.search(r"struct susfs_fuse_inode susfs_path;\n#endif\n\};",(root/"fs/fuse/fuse_i.h").read_text())
    assert "susfs_path.o susfs_fuse.o" in (root/"fs/Makefile").read_text()
    for file in ["fs/fuse/readdir.c","fs/fuse/backing.c","fs/inode.c","include/linux/fs.h","include/linux/mount.h"]:
        assert (root/file).read_bytes()==kernel_source(root, file).encode(),file
    cc=["clang","-std=gnu11","-O1","-g","-pthread","-Wall","-Wextra","-Werror","-Wno-unused-function",
        "-Wno-unused-parameter","-Wno-sign-compare","-fsanitize=address,undefined","-fno-sanitize-recover=undefined",
        "-fno-omit-frame-pointer","-I",str(out),"-I",str(root/"scripts/susfs")]
    flags=["-DCONFIG_KSU_SUSFS=1","-DCONFIG_KSU=1","-DCONFIG_KSU_DEVON_SELINUX_POLICY=1","-DCONFIG_ARM64=1",
           "-DCONFIG_64BIT=1","-DCONFIG_THREAD_INFO_IN_TASK=1","-DCONFIG_KSU_SUSFS_SUS_PATH=1","-DCONFIG_COMPAT=1"]
    fixture=root/"scripts/susfs/test_fuse_path_index.c"
    results={"scope":__doc__,"compiler":run(["clang","--version"]).splitlines()[0],"runs":[]}
    for bpf in [False,True]:
        for dax in [False,True]:
            name=f"fuse-bpf-{int(bpf)}-dax-{int(dax)}"
            selected=flags+(["-DCONFIG_FUSE_BPF=1"] if bpf else [])+(["-DCONFIG_FUSE_DAX=1"] if dax else [])
            (out/(name+"-compile.log")).write_text(run(cc+selected+[str(fixture),"-o",str(out/name)]))
            result=run([str(out/name)])
            (out/(name+".log")).write_text(result)
            results["runs"].append({"name":name,"exit":0,"result":result.strip()})
            print(name+": "+result.strip())
    rejected_modes=[]
    for name, extra in [("no-path", ["-UCONFIG_KSU_SUSFS_SUS_PATH"]),
                        ("no-susfs", ["-UCONFIG_KSU_SUSFS"]),
                        ("no-provider", ["-UCONFIG_KSU_DEVON_SELINUX_POLICY"]),
                        ("module-core", ["-DMODULE=1"])]:
        # Include the real index directly even when its selection flag is absent.
        base=(root/"scripts/susfs/test_path_consumers.c").read_text()
        prefix=base.split("#ifdef CONFIG_KSU_SUSFS_SUS_PATH\n#include \"fuse_index_model.h\"")[0]
        probe=prefix+'\n#include "fuse_index_model.h"\n#include "fuse_index_under_test.h"\n'
        r=subprocess.run(cc+flags+extra+["-fsyntax-only","-x","c","-"],input=probe,capture_output=True,text=True)
        (out/("reject-"+name+".log")).write_text(r.stderr)
        assert r.returncode and "requires its checked built-in SUS_PATH route" in r.stderr,name
        rejected_modes.append(name)
    results["configuration_rejections"]=rejected_modes
    # FUSE caller bodies may be modular; the index itself stays built-in.
    common=(root/"scripts/susfs/test_path_consumers.c").read_text().replace('#include "fuse_index_under_test.h"','#include <linux/susfs_fuse.h>')
    common=common.replace("int main(void)","int base_consumer_main(void)")
    provider_prefix=fixture.read_text().split("#undef main\n",1)[1].split('#include "fuse_lifecycle.h"',1)[0]
    provider_prefix=provider_prefix.replace("live_allocs, clears, bpf_puts, cache_adds, plus_links, plus_forgets", "live_allocs, clears, bpf_puts")
    probe=common+"\n"+provider_prefix+'\n#include "fuse_lifecycle.h"\n'
    (out/"fuse-caller-probe.c").write_text(probe)
    for mode,extra in [("builtin",["-DCONFIG_FUSE_FS=1"]),("module",["-DCONFIG_FUSE_FS_MODULE=1","-DMODULE=1"])]:
        (out/("fuse-caller-"+mode+".log")).write_text(run(cc+flags+extra+["-DCONFIG_FUSE_BPF=1","-DCONFIG_FUSE_DAX=1","-fsyntax-only",str(out/"fuse-caller-probe.c")]))
    results["caller_syntax"]= "Actual lifecycle callers compile with the real typed API as builtin and module; no real Kbuild/module link"
    # Mutate the real lookup/eviction functions only in isolated generated test headers.
    controls=[("reject-old-cache-gap","readdir_under_test.h","return susfs_fuse_is_hidden(dir->d_sb, ino);","return false;"),
              ("reject-retained-evicted-entry","fuse_lifecycle.h","susfs_fuse_inode_forget(&fi->susfs_path);","(void)fi;")]
    for name,file,old,new in controls:
        original=(out/file).read_text();assert old in original
        try:
            (out/file).write_text(original.replace(old,new))
            binary=out/name
            (out/(name+"-compile.log")).write_text(run(cc+flags+[str(fixture),"-o",str(binary)]))
            r=subprocess.run([str(binary)],capture_output=True,text=True)
            (out/(name+".log")).write_text(r.stdout+r.stderr)
            assert r.returncode and ("Assertion" in r.stderr or "AddressSanitizer" in r.stderr),name
        finally: (out/file).write_text(original)
    results["mutation_controls"]="Old FUSE cache miss and retained eviction entries both fail the actual-source suite"
    results["layout"]="SUSFS entry is 40 bytes on the 64-bit host; private FUSE inode grows only with PATH; public/native inode/vfsmount and payload layouts unchanged"
    files=[Path(__file__).resolve(),fixture,root/"scripts/susfs/test_path_consumers.py",root/"scripts/susfs/test_path_consumers.c",
           root/"scripts/susfs/fuse_index_model.h",root/"scripts/susfs/test_selinux_adapter.py",root/"scripts/susfs/test_setresuid_task_state.py",
           root/"scripts/susfs/source-pins.json",root/"fs/susfs_fuse.c",root/"include/linux/susfs_fuse.h",root/"fs/fuse/inode.c",
           root/"fs/fuse/fuse_i.h",root/"fs/fuse/readdir.c",root/"fs/fuse/backing.c",root/"fs/readdir.c",root/"fs/namei.c",root/"fs/susfs_path.c",
           root/"fs/Makefile",root/"include/linux/list.h",root/"include/linux/hashtable.h",root/"include/linux/fs.h",root/"include/linux/mount.h",
           root/"include/linux/susfs.h",root/"include/linux/susfs_def.h",root/"include/linux/ksu_susfs.h"]
    results["inputs"]={str(f.relative_to(root)):digest(f) for f in files}
    (out/"results.json").write_text(json.dumps(results,indent=2)+"\n")
    print("FUSE live-index source/host checks pass; aggregate PATH and native kernel/device validation remain gated.")


if __name__=="__main__": main()
