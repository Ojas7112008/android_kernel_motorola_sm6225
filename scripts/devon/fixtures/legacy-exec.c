static int do_execveat_common(int fd, struct filename *filename,
			      struct user_arg_ptr argv,
			      struct user_arg_ptr envp,
			      int flags)
{
#ifdef CONFIG_KSU_MANUAL_HOOK
    int retval;

    ksu_handle_execveat(&fd, &filename, &argv, &envp, &flags);

    retval = __do_execve_file(fd, filename, argv, envp, flags, NULL);

    ksu_handle_post_execveat(&fd, &filename, &argv, &envp, &flags,
                             &retval);

    return retval;
#else
    return __do_execve_file(fd, filename, argv, envp, flags, NULL);
#endif
}
