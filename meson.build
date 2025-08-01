project('libwacom', 'c',
	version: '2.16.1',
	license: 'HPND',
	default_options: [ 'c_std=gnu99', 'warning_level=2' ],
	meson_version: '>= 0.57.0')

dir_bin      = get_option('prefix') / get_option('bindir')
dir_data     = get_option('prefix') / get_option('datadir') / 'libwacom'
dir_etc      = get_option('prefix') / get_option('sysconfdir') / 'libwacom'
dir_src      = meson.current_source_dir() / 'libwacom'
dir_src_data = meson.current_source_dir() / 'data'
dir_test     = meson.current_source_dir() / 'test'
dir_sys_udev = get_option('prefix') / 'lib' / 'udev'

dir_udev = get_option('udev-dir')
if dir_udev == ''
	dir_udev = dir_sys_udev
endif

fs = import('fs')
pymod = import('python')
python = pymod.find_installation('python3', required: true)

# Do not modify this, use symbol versioning instead.
libwacom_lt_c=9
libwacom_lt_r=0
libwacom_lt_a=0
# convert ltversion to soname
libwacom_so_version = '@0@.@1@.@2@'.format((libwacom_lt_c - libwacom_lt_a),
                                            libwacom_lt_a, libwacom_lt_r)

# Compiler setup
cc = meson.get_compiler('c')
cflags = cc.get_supported_arguments(
	'-Wno-unused-parameter',
	'-Wmissing-prototypes',
	'-Wstrict-prototypes',
	'-Wundef',
	'-Wlogical-op',
	'-Wpointer-arith',
	'-Wuninitialized',
	'-Winit-self',
	'-Wstrict-prototypes',
	'-Wimplicit-fallthrough',
	'-Wredundant-decls',
	'-Wincompatible-pointer-types',
	'-Wformat=2',
	'-Wsign-compare',
	'-Wmissing-declarations',
)
add_project_arguments(cflags, language: 'c')

# Dependencies
pkgconfig    = import('pkgconfig')
dep_gudev    = dependency('gudev-1.0')
dep_glib     = dependency('glib-2.0')
dep_libevdev = dependency('libevdev')

includes_include = include_directories('include')
includes_src = include_directories('libwacom')

# config.h
config_h = configuration_data()
config_h.set10('HAVE_G_MEMDUP2',
	       cc.has_function('g_memdup2',
			       dependencies: dep_glib))

#################### libwacom.so ########################
src_libwacom = [
	'include/linux/input-event-codes.h',
	'libwacom/libwacom.h',
	'libwacom/libwacomint.h',
	'libwacom/libwacom.c',
	'libwacom/libwacom-error.c',
	'libwacom/libwacom-database.c',
]

deps_libwacom = [
	    dep_gudev,
	    dep_glib,
	    dep_libevdev,
]

inc_libwacom = [
	     includes_include,
	     includes_src,
]

mapfile = dir_src / 'libwacom.sym'
version_flag = '-Wl,--version-script,@0@'.format(mapfile)
lib_libwacom = shared_library('wacom',
			      src_libwacom,
			      include_directories: inc_libwacom,
			      dependencies: deps_libwacom,
			      version: libwacom_so_version,
			      link_args: version_flag,
			      link_depends: mapfile,
			      c_args: [
				'-DG_LOG_DOMAIN="@0@"'.format(meson.project_name()),
				'-DDATADIR="@0@"'.format(dir_data),
				'-DETCDIR="@0@"'.format(dir_etc),
			      ],
			      gnu_symbol_visibility: 'hidden',
			      install: true)
dep_libwacom = declare_dependency(link_with: lib_libwacom)

install_headers('libwacom/libwacom.h', subdir: 'libwacom-1.0/libwacom')

pkgconfig.generate(filebase: 'libwacom',
		   name: 'libwacom',
		   description: 'Wacom model feature query library',
		   version: meson.project_version(),
		   subdirs: 'libwacom-1.0',
		   requires_private: deps_libwacom,
		   libraries: lib_libwacom)

#################### data files ########################

install_subdir('data',
	       install_dir: dir_data,
	       strip_directory: true,
	       exclude_files: ['wacom.example',
			       'layouts/README.md'])

test('files-in-git',
     find_program('test/check-files-in-git.sh'),
     args: [meson.current_source_dir()],
     suite: ['all'])

############### tools ###########################

tools_cflags = ['-DDATABASEPATH="@0@"'.format(dir_src_data)]

executable('libwacom-list-local-devices',
	   'tools/list-local-devices.c',
	   dependencies: [dep_libwacom, dep_glib, dep_gudev],
	   include_directories: [includes_src],
	   install: true)

# The non-installed version of list-local-devices uses the git tree's data files
executable('list-local-devices',
	   'tools/list-local-devices.c',
	   dependencies: [dep_libwacom, dep_glib, dep_gudev],
	   include_directories: [includes_src],
	   c_args: tools_cflags,
	   install: false)

updatedb = configure_file(input: 'tools/libwacom-update-db.py',
			  output: '@BASENAME@',
			  copy: true,
			  install: true,
			  install_dir: dir_bin)
hwdb = custom_target('hwdb',
	             command: [python, updatedb, '--buildsystem-mode', dir_src_data],
	             capture: true,
	             output: '65-libwacom.hwdb',
	             install: true,
	             install_dir: dir_udev / 'hwdb.d')

configure_file(input: 'tools/65-libwacom.rules.in',
	       output: '65-libwacom.rules',
	       copy: true,
	       install: true,
	       install_dir: dir_udev / 'rules.d')

# The non-installed version of list-devices uses the git tree's data files
debug_device = executable('debug-device',
			  'tools/debug-device.c',
			  dependencies: [dep_libwacom, dep_glib],
			  include_directories: [includes_src],
			  c_args: tools_cflags,
			  install: false)
testdevices = {
	'huion-h640p': 'usb|256c|006d|HUION Huion Tablet_H640P Pad',
	'huion-dial-2': 'usb|256c|006e||HUION_T216',
	'wacom-cintiq-pro-13': 'usb|056a|034f',
	'wacom-intuos4': 'usb|056a|00bb',
	'wacom-isdv4-0148': 'usb|056a|0148',
}
foreach name : testdevices.keys()
	match = testdevices[name]
	test(f'debug-device-@name@',
	debug_device,
	args: [match],
	suite: ['all'])
endforeach

# The non-installed version of list-devices uses the git tree's data files
list_devices = executable('list-devices',
			  'tools/list-devices.c',
			  dependencies: [dep_libwacom, dep_glib],
			  include_directories: [includes_src],
			  c_args: tools_cflags,
			  install: false)
test('list-devices', list_devices, suite: ['all'])

# The installed version of list-devices uses the installed data files
executable('libwacom-list-devices',
	   'tools/list-devices.c',
	   dependencies: [dep_libwacom, dep_glib],
	   include_directories: [includes_src],
	   install: true)

list_compatible_styli = executable('list-compatible-styli',
				   'tools/list-compatible-styli.c',
				   dependencies: [dep_libwacom, dep_glib],
				   include_directories: [includes_src],
				   c_args: tools_cflags,
				   install: false)
test('list-compatible-styli', list_compatible_styli, suite: ['all'])

install_man(configure_file(input: 'tools/libwacom-list-local-devices.man',
			   output: '@BASENAME@.1',
			   copy: true))

install_man(configure_file(input: 'tools/libwacom-list-devices.man',
			   output: '@BASENAME@.1',
			   copy: true))

install_man(configure_file(input: 'tools/libwacom-show-stylus.man',
			   output: '@BASENAME@.1',
			   copy: true))

showstylus_config = configuration_data()
showstylus_config.set('DATADIR', dir_data)
showstylus_config.set('ETCDIR', dir_etc)
configure_file(output: 'libwacom-show-stylus',
	       input: 'tools/show-stylus.py',
	       configuration: showstylus_config,
	       install_dir: dir_bin,
	       install: true)

############### docs ###########################
docs_feature = get_option('documentation')
doxygen = find_program('doxygen', required: docs_feature)
if doxygen.found()
	src_doxygen = [
		dir_src / 'libwacom.h',
	]
	doc_config = configuration_data()
	doc_config.set('PACKAGE_NAME', meson.project_name())
	doc_config.set('PACKAGE_VERSION', meson.project_version())
	doc_config.set('TOPSRCDIR', meson.current_source_dir())

	doxyfile = configure_file(input: 'doc/doxygen.conf.in',
				  output: 'doxygen.conf',
				  configuration: doc_config)
	custom_target('doxygen',
		      input: [doxyfile] + src_doxygen,
		      output: ['html'],
		      command: [doxygen, doxyfile],
		      install: false,
		      build_by_default: true)
endif
############# tests ############################

if get_option('tests').enabled()
	dep_dl      = cc.find_library('dl')

	tests_cflags = ['-DTOPSRCDIR="@0@"'.format(meson.current_source_dir())]

	test_load = executable('test-load',
			       'test/test-load.c',
			       dependencies: [dep_libwacom, dep_glib],
			       include_directories: [includes_include, includes_src],
			       c_args: tests_cflags,
			       install: false)
	test('test-load', test_load, suite: ['all'])

	test_dbverify = executable('test-dbverify',
				   'test/test-dbverify.c',
				   dependencies: [dep_libwacom, dep_glib],
				   include_directories: [includes_src],
				   c_args: tests_cflags,
				   install: false)
	test('test-dbverify', test_dbverify, suite: ['all'])

	test_tablet_validity = executable('test-tablet-validity',
					  'test/test-tablet-validity.c',
					  dependencies: [dep_libwacom, dep_glib],
					  include_directories: [includes_src],
					  c_args: tests_cflags,
					  install: false)
	test('test-tablet-validity', test_tablet_validity, suite: ['all'])

	test_stylus_validity= executable('test-stylus-validity',
					 'test/test-stylus-validity.c',
					 dependencies: [dep_libwacom, dep_glib],
					 include_directories: [includes_src],
					 c_args: tests_cflags,
					 install: false)
	test('test-stylus-validity', test_stylus_validity, suite: ['all'])

	valgrind = find_program('valgrind', required : false)
	if valgrind.found()
		valgrind_suppressions_file = dir_test / 'valgrind.suppressions'
		add_test_setup('valgrind',
			       exclude_suites: ['not-in-valgrind'],
			       exe_wrapper: [valgrind,
					     '--leak-check=full',
					     '--gen-suppressions=all',
					     '--error-exitcode=3',
					     '--suppressions=' + valgrind_suppressions_file ],
			       timeout_multiplier : 100)
	else
		message('valgrind not found, disabling valgrind test suite')
	endif

	if get_option('b_sanitize') == 'none'
		env = environment()
		env.set('MESON_SOURCE_ROOT', meson.current_source_dir())
		env.set('LIBWACOM_HWDB_FILE', hwdb.full_path())
		env.set('LD_LIBRARY_PATH', fs.parent(lib_libwacom.full_path()))
		pymod.find_installation(modules: ['libevdev', 'pyudev', 'pytest'])
		pytest = find_program('pytest-3', 'pytest')
		pytest_files = [
			'test_data_files.py',
			'test_libwacom.py',
			'test_svg.py',
			'test_udev_rules.py',
		]
		foreach f: pytest_files
			test('pytest @0@'.format(f),
			     pytest,
			     args: ['--verbose',
				     '-rfES',
				     '--log-level=DEBUG',
				     '--log-file', meson.project_build_root() / 'meson-logs' / 'pytest.log',
				     '--log-file-level=DEBUG',
				     meson.current_source_dir() / 'test' / f,
			     ],
			     env: env,
			     timeout: 60,
			     suite: ['all', 'not-in-valgrind'])
		endforeach
	endif
endif

# This is a non-optional test
lt_version = '@0@:@1@:@2@'.format( libwacom_lt_c, libwacom_lt_r, libwacom_lt_a)
test_ltversion = executable('test-ltversion',
			    'test/test-ltversion.c',
			    c_args: ['-DLIBWACOM_LT_VERSION="@0@"'.format(lt_version)],
			    install: false)
test('test-ltversion', test_ltversion, suite: ['all'])



############ output files ############
configure_file(output: 'config.h', configuration: config_h)

# vim: set noexpandtab tabstop=8 shiftwidth=8:
