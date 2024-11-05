project('mqttd', 'c')

include_dir = include_directories('include')

source = ['src/main.c', 'src/hash.c']
executable('mqttd', source, include_directories: include_dir)

hash_test = executable('hash_test', 'src/hash.c', 'tests/hash.c', include_directories: include_dir)
test('hash test', hash_test)
