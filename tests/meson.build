tests_to_pass = [
    'create',
    'deinit',
    'init',
    'issue_1',
    'mkdir',
    'open',
    'read',
    'rmdir',
    'seek',
    'unlink',
    'write',
]

tests_to_fail = [
]

foreach name : tests_to_pass
    exe = executable(name, f'pass_@name@_test.c',
        build_by_default: false,
        dependencies: [ramfs_dep],
    )
    test(f'pass_@name@', exe, should_fail: false)
endforeach

foreach name : tests_to_fail
    exe = executable(name, f'fail_@name@_test.c',
        build_by_default: false,
        dependencies: [ramfs_dep],
    )
    test(f'pass_@name@', exe, should_fail: true)
endforeach
