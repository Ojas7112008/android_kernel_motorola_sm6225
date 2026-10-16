# Devon SELinux type-creation allocation checkpoint

**Historical checkpoint:** Source selection and SELinux ownership have been superseded by the [automatic-hook checkpoint](SUSFS_2_3_SELINUX_AUTOHOOKS.md). Original source pins/results below are preserved as historical evidence.

Date: 2026-10-13. The user approved the next step after the [policy lifecycle checkpoint](SUSFS_2_3_SELINUX_LIFECYCLE.md). This change fixes the target add_type allocation contract and propagates critical initialization failures; it does not certify the whole backport or remove BakaSU's build restriction.

## Implemented change

The authoritative [BakaSU integration patch](../.github/patches/bakasu-devon-selinux-adapter-f443.patch) remains based on f4435f007b16f61705089ff982e3f981a6017979 and still changes seven files. This checkpoint adds a devon-specific implementation in kernel/selinux/sepolicy.c and error propagation in kernel/selinux/rules.c. Immutable source references and Motorola's protected branch are unchanged.

The audited Motorola policydb uses flex_array objects for type attribute maps, type pointers and names; role type sets are extensible bitmaps. The original add_type incremented nprim and inserted its hash entry before allocating all replacement arrays. It leaked on failures and ignored bitmap/copy errors. The target implementation now:

1. Validates names, type-count bounds, required containers and their element sizes/capacities, while retaining allocation-free success for an existing symbol.
2. Allocates the new datum/key, three replacement arrays and all required backing pages privately.
3. Copies only logical old entries, checks every write and creates the new type's self bitmap.
4. Deep-copies role type bitmaps and checks extension of each copy. Published role bitmaps remain untouched during preparation.
5. Inserts the hash entry as the last operation that can fail. On success, transfers the arrays, bitmap ownership and count without further allocations.
6. On failure, destroys only private bitmap nodes and containers, then frees the datum/key. Borrowed old map nodes and original role sets remain owned by the original policy.

Caller contract: the devon policy transaction owns an unpublished policy exclusively. Type mutation does not introduce a new lock or publish into the live kernel policy itself. All in-tree type/attribute callers were traced to default initialization or userspace batches in rules.c, which use private copies in this adapter mode.

The implementation is selected only for KSU_COMPAT_SUSFS_DEVON_SELINUX_ADAPTER. An explicit compile-time check rejects modern/non-flex layouts rather than silently treating them as the audited Motorola representation. Other BakaSU modes retain their original add_type branch.

The default rule helper now checks the two required ksu_type operations (the KSU domain and file type) and the permissive-map operation. A reported failure discards the candidate and returns -EINVAL through the existing target setup/error path, so SID/credential caching is skipped. This uses the primitives' existing boolean API; it does not add a new userspace error ABI. Other optional/default rule behavior remains as before. Non-adapter default statements were compared with the pin after preprocessing and match, apart from the ignored helper return.

Type-to-attribute binding and constraint bitmap mutations after a successful type append remain separate operations. This checkpoint guarantees the add_type failure contract; it does not claim the entire ksu_type command or every bitmap mutation is atomic. The existing batch partial-success convention remains unchanged.

## Executed checks

Run the existing host runner:

```sh
python3 scripts/susfs/test_selinux_adapter.py
```

The new [fixture](../scripts/susfs/test_selinux_type_creation.c) compiles the actual Motorola lib/flex_array.c, reciprocal-division implementation, selected ebitmap functions and hashtab implementation, plus the actual patched add_type/helper. Include dependencies, allocation/cache services, bit-scan helpers and scheduler calls are supplied by the host harness. Ownership-relevant type/role structs come from the real vendor header; the containing policy model covers only fields under test and is not an ARM64 ABI assertion.

Clang 21.1.8 with ASan/UBSan passes all seven positive test configurations: three original adapter configurations, three lifecycle/init paths and the new native-primitive type test. The new cases exercise:

| Old types / spare capacity / roles | Kind | Allocation failures and successful retries |
|---|---|---|
| 1 / 0 / 2 | Type | 10 |
| 254 / 0 / 2 | Type; bitmap array crosses base-page capacity | 11 |
| 509 / 0 / 2 | Type; pointer arrays cross base-page capacity | 16 |
| 384 / 0 / 2 | Attribute; bitmap-node boundary | 14 |
| 768 / 0 / 2 | Type; partial multi-node copies and extensions | 22 |
| 2 / 1000 / 2 | Type; source capacity exceeds logical count | 10 |
| 1 / 0 / 0 | Attribute; no role array | 7 |

All 90 allocation-failure cases leave count, hash membership, array pointers, original entries and role bitmap state unchanged, with balanced allocated blocks/bytes. Each retries successfully on the same policy. Nine additional injected flex-array write errors check defensive rollback, including preservation of borrowed bitmap nodes. Existing-symbol, invalid-name/count/container-layout and unsupported-layout checks pass. The lifecycle fixtures also verify rollback when either required type call or the permissive operation reports failure.

The pinned unmodified add_type was compiled against the same vendor primitives. Three expected negative cases reproduce a changed type count on failure; the latter two also show leaked datum/key allocations. The harness restores only that broken host fixture for cleanup. This is a reproduced baseline defect, not a successful baseline allocation contract or a modified upstream reference.

Strict complete-patch application, reverse applicability and reconstructed-source whitespace pass. The actual Kbuild probe still detects the adapter and then make exits 2 at the unmodified susfs_compat.mk:20. The guard remains SHA-256 25eb93fe03d155f5cb19f1de8a4e291d88f0016ff0f7e53b558f63fae1a9e2a7.

The external BakaSU patch's kernel checkpatch exits 1: 156 indentation errors and 429 warnings. All errors require tabs, conflicting with BakaSU's explicit four-space/no-tab format. Native formatting is retained, with no claim of a clean kernel-style result. The host build suppresses sign-compare warnings in the extracted vendor primitives; other enabled warnings are errors. The initial new fixture needed a no-op cond_resched declaration for the native hashtab code; after that harness correction all checks passed.

Current logs and source hashes are in .backport-context/audit/selinux-adapter/. The previous passing lifecycle result and patch were preserved before editing under .backport-context/audit/selinux-type-creation/. Its source-review.json and checkpatch.log record additional checks.

## Remaining boundary

Next review SID/domain cache and credential readiness, then continue exec/init and the other BakaSU hook interfaces. Keep attribute/constraint bitmap error propagation and optional rule handling on the validation list: this targeted fix does not resolve all mutation primitives. Real policy parsing/serialization, alias/attribute semantics, kernel fault injection/concurrency/lockdep/RCU, production CFI/LTO and device behavior still require validation.

The full SUSFS-enabled kernel cannot yet be built through the intentional upstream restriction. No compatibility bypass, SUSFS version change, full ARM64 build/link, image operation, push, CI dispatch, boot or flash was performed. Host primitive tests do not establish a safe or bootable kernel.
