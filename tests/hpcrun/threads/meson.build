
_threadmodels = {
  'pthread': executable('tstexe-pthread', files('pthread.c'), dependencies: [threads_dep, math_dep]),
  'openmp': executable('tstexe-openmp', files('openmp.c'), dependencies: [openmp_dep, math_dep]),
  'cpp-thread': executable('tstexe-cpp-thread', files('cpp-thread.cpp'), dependencies: [threads_dep, math_dep]),
  'c11-thread': executable('tstexe-c11-thread', files('c11-thread.c'), dependencies: [threads_dep, math_dep]),
}
foreach _threadmodel, _tstexe : _threadmodels
  # FIXME: C11 thread functions aren't intercepted in current versions, which means the tests will
  # fail when using the C11 threading model.
  _should_fail = _threadmodel == 'c11-thread'

  test(
    f'Threads are tracked for @_threadmodel@',
    find_program(files('tst-threads-tracked')),
    args: [hpctesttool, hpcrun, _tstexe],
    suite: 'hpcrun',
    depends: hpcrun_test_depends,
    env: hpcrun_test_env,
    should_fail: _should_fail,
  )
  test(
    f'Threads are tracked for @_threadmodel@ w/o auditor',
    find_program(files('tst-threads-tracked-noaudit')),
    args: [hpctesttool, hpcrun, _tstexe],
    suite: 'hpcrun',
    depends: hpcrun_test_depends,
    env: hpcrun_test_env,
    should_fail: _should_fail,
  )
endforeach
