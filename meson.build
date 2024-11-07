project('mqttd', 'c')

include_dir = include_directories('include')

thread_dep = dependency('threads')

source = ['src/main.c', 'src/hash.c', 'src/server.c']
executable('mqttd', source, include_directories: include_dir, dependencies: thread_dep)

hash_test = executable('hash_test', 'src/hash.c', 'tests/hash.c', include_directories: include_dir)
test('hash test', hash_test)
