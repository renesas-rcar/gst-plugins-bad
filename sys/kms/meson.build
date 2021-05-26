kmssink_sources = [
  'gstkmsallocator.c',
  'gstkmsbufferpool.c',
  'gstkmssink.c',
  'gstkmsutils.c',
]

libdrm_dep = dependency('libdrm', version : '>= 2.4.55', required : get_option('kms'))
if libdrm_dep.found()
  # Check rcar_du_drm.h to use for write-back
  if cc.has_header (
    'drm/rcar_du_drm.h',
    args : gst_plugins_bad_args)
    cdata.set ('HAVE_RCAR_EXT', 1)
  endif

  # Get RECORD_OPTION. When enable this option, kmssink will record display frame into file
  if get_option('enable-recorder')
    cdata.set('ENABLE_RECORDER', 1)
  endif

  # check mmngr library and function to use for write-back
  lib_mmngr = cc.find_library('mmngr', required : false)
  have_mmngr = cc.has_function('mmngr_alloc_in_user_ext', dependencies : lib_mmngr)
  if have_mmngr
    cdata.set('HAVE_MMNGR', 1)
    gstkmssink = library('gstkms',
      kmssink_sources,
      c_args : gst_plugins_bad_args,
      include_directories : [configinc],
      dependencies : [gstbase_dep, gstvideo_dep, gstallocators_dep, libdrm_dep, lib_mmngr],
      install : true,
      install_dir : plugins_install_dir,
    )
  else
    gstkmssink = library('gstkms',
      kmssink_sources,
      c_args : gst_plugins_bad_args,
      include_directories : [configinc],
      dependencies : [gstbase_dep, gstvideo_dep, gstallocators_dep, libdrm_dep],
      install : true,
      install_dir : plugins_install_dir,
    )
  endif
  pkgconfig.generate(gstkmssink, install_dir : plugins_pkgconfig_install_dir)

endif
