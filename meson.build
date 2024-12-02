project('mqttd', 'c')

include_dir = include_directories('include')

thread_dep = dependency('threads')

server_source = ['src/server_main.c', 'src/hash.c', 'src/server.c', 'src/utils.c']
executable('mqttd', server_source, include_directories: include_dir, dependencies: thread_dep)

client_source = ['src/client_main.c', 'src/hash.c', 'src/client.c', 'src/utils.c']
executable('mqttc', client_source, include_directories: include_dir, dependencies: thread_dep)

hash_test = executable('hash_test', 'src/hash.c', 'tests/hash.c', include_directories: include_dir)
test('hash test', hash_test)
