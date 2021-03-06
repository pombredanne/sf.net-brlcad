set(SIG_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${FB_INCLUDE_DIRS}
  ${FFT_INCLUDE_DIRS}
  )
LIST(REMOVE_DUPLICATES SIG_INCLUDE_DIRS)
include_directories(${SIG_INCLUDE_DIRS})

BRLCAD_ADDEXEC(damdf damdf.c libbu)
BRLCAD_ADDEXEC(dauto dauto.c libbu)
BRLCAD_ADDEXEC(dauto2 dauto2.c libbu)
BRLCAD_ADDEXEC(dconv dconv.c "libbu;libfft")
BRLCAD_ADDEXEC(ddisp ddisp.c libfb)
BRLCAD_ADDEXEC(dfft "dfft.c;interp.c;butter.c" "libbu;libfft")
BRLCAD_ADDEXEC(dmod dmod.c "libbu;${M_LIBRARY}")
BRLCAD_ADDEXEC(dpeak dpeak.c libbu)
BRLCAD_ADDEXEC(dsel dsel.c libbu)
BRLCAD_ADDEXEC(dstats dstats.c "libbu;${M_LIBRARY}")
BRLCAD_ADDEXEC(dwin "dwin.c;coswin.c;hamwin.c" "libbu;${M_LIBRARY}")
BRLCAD_ADDEXEC(ihist ihist.c libbu)
BRLCAD_ADDEXEC(imod imod.c "libbu;${M_LIBRARY}")
BRLCAD_ADDEXEC(istats istats.c "libbu;${M_LIBRARY}")
BRLCAD_ADDEXEC(smod smod.c "libbu;${M_LIBRARY}")
BRLCAD_ADDEXEC(umod umod.c "libbu;${M_LIBRARY}")
BRLCAD_ADDEXEC(ustats ustats.c "libbu;${M_LIBRARY}")

# Deprecated
BRLCAD_ADDEXEC(a-d a-d.c libbu)
BRLCAD_ADDEXEC(bw-d bw-d.c libbu)
BRLCAD_ADDEXEC(c-d c-d.c "libbu;${M_LIBRARY}")
BRLCAD_ADDEXEC(d-a d-a.c libbu)
BRLCAD_ADDEXEC(d-bw d-bw.c libbu)
BRLCAD_ADDEXEC(d-f d-f.c libbu)
BRLCAD_ADDEXEC(d-i d-i.c libbu)
BRLCAD_ADDEXEC(d-u d-u.c libbu)
BRLCAD_ADDEXEC(d2-c d2-c.c libbu)
BRLCAD_ADDEXEC(f-d f-d.c libbu)
BRLCAD_ADDEXEC(f-i f-i.c libbu)
BRLCAD_ADDEXEC(i-a i-a.c libbu)
BRLCAD_ADDEXEC(i-d i-d.c libbu)
BRLCAD_ADDEXEC(i-f i-f.c libbu)
BRLCAD_ADDEXEC(u-a u-a.c libbu)
BRLCAD_ADDEXEC(u-bw u-bw.c libbu)
BRLCAD_ADDEXEC(u-d u-d.c libbu)
BRLCAD_ADDEXEC(u-f u-f.c libbu)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
