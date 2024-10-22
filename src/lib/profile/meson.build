profile_srcs = files(
  'accumulators.cpp',
  'attributes.cpp',
  'context.cpp',
  'dataclass.cpp',
  'expression.cpp',
  'finalizer.cpp',
  'finalizers/denseids.cpp',
  'finalizers/directclassification.cpp',
  'finalizers/kernelsyms.cpp',
  'finalizers/logical.cpp',
  'finalizers/struct.cpp',
  'lexical.cpp',
  'metric.cpp',
  'module.cpp',
  'mpi/accumulate-thread.cpp',
  'packedids.cpp',
  'pipeline.cpp',
  'scope.cpp',
  'sink.cpp',
  'sinks/hpctracedb2.cpp',
  'sinks/metadb.cpp',
  'sinks/metricsyaml.cpp',
  'sinks/packed.cpp',
  'sinks/sparsedb.cpp',
  'source.cpp',
  'sources/hpcrun4.cpp',
  'sources/packed.cpp',
  'stdshim/atomic.cpp',
  'stdshim/futex-detail.c',
  'stdshim/shared_mutex.cpp',
  'util/file-posix.cpp',
  'util/log.cpp',
  'util/lzmastream.cpp',
  'util/once.cpp',
  'util/ragged_vector.cpp',
  'util/stable_hash.cpp',
  'util/xml.cpp',
)
profile_srcs += configure_file(output: 'static.data.cpp', input: 'static.data.cpp.in',
  configuration: {
    'FORMATS_MD': fs.read(meson.project_source_root() / 'doc/FORMATS.md'),
    'METRICS_YAML': fs.read(meson.project_source_root() / 'doc/METRICS.yaml'),
  })

profile_deps = [
  libdw_dep,
  libelf_dep,
  lzma_dep,
  openmp_dep,
  xerces_dep,
  xxhash_dep,
  yaml_cpp_dep,
]

profile_standalone_srcs = files(
  'mpi/standalone.cpp',
)
profile_standalone_srcs += profile_srcs
