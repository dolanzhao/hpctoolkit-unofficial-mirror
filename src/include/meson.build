# Construct a summary message that indicates all the features we have available
# and their respective versions.
features_msg = ''

features_msg += 'PAPI support: @0@\n'.format(papi_dep.found() ? 'yes' : 'no')
if papi_dep.found()
  features_msg += '''
- PAPI: @0@
'''.strip().format(papi_dep.version()) + '\n'
endif

features_msg += 'NVIDIA CUDA support: @0@\n'.format(cupti_dep.found() ? 'yes' : 'no')
if cupti_dep.found()
  features_msg += '''
- CUDA: @0@
'''.strip().format(cupti_dep.version()) + '\n'
endif

features_msg += 'Torch Monitor support: @0@\n'.format(torch_monitor_dep.found() ? 'yes' : 'no')

features_msg += 'AMD ROCm support: @0@\n'.format(rocm_dep.found() ? 'yes' : 'no')
if rocm_dep.found()
  features_msg += '''
- ROCm: @0@
'''.strip().format(rocm_dep.version()) + '\n'
endif

features_msg += 'Intel Level Zero support: @0@\n'.format(level0_dep.found() ? 'yes' : 'no')
if level0_dep.found()
  features_msg += '''
- Level Zero: @0@
'''.strip().format(level0_dep.version()) + '\n'
  if gtpin_dep.found()
    features_msg += '''
- GTPin: @0@
'''.strip().format(gtpin_dep.version() == 'unknown' ? 'enabled' : gtpin_dep.version()) + '\n'
  endif
endif

features_msg += 'OpenCL support: @0@\n'.format(opencl_dep.found() ? 'yes' : 'no')
features_msg += 'Python support: @0@\n'.format(python_inst.found() ? 'yes' : 'no')

# Generate the final version information source file. The header also needs to be copied beside
# it for it to compile properly.
version_cpp = configure_file(output: 'hpctoolkit-version.cpp.vcs.in', input: 'hpctoolkit-version.cpp.in',
  configuration: {'FEATURES_MESSAGE': features_msg, 'VERSION': meson.project_version(), 'VCS_TAG': '@VCS_TAG@'})
version_cpp = [
  vcs_tag(output: 'hpctoolkit-version.cpp', input: version_cpp),
  fs.copyfile('hpctoolkit-version.h'),
]
