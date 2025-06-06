#
# Windows specific
#

WIN32PATH ?= $(DESTDIR)

clean-win32-resource-embed:

clean: clean-win32-resource-embed

.PHONY: clean-win32-resource-embed

define win32-resource-embed-macro=
$(1): configure.stamp $(foreach filename,$($(2)), $(srcdir)/$(filename)) $($(2)_SOURCE)
	$(QUIET)echo '    GENERATE' $$@
	$(QUIET)mkdir -p $$(dir $$@)
	$(QUIET)echo -n '' > $$@
	$(QUIET)echo $$(foreach filename,$$($(2)),$$(filename)) | sed -e 's/ /\n/g' | sed -E 's/(.*\/)(.+)/\2 FILE "\1\2"/g' >> $$@

dists/scummvm.o: $(1)

clean-win32-resource-embed-$(1):
	$(RM) $(1)

clean-win32-resource-embed: clean-win32-resource-embed-$(1)

.PHONY: clean-win32-resource-embed-$(1)

endef

$(eval $(call win32-resource-embed-macro,dists/scummvm_rc_engine_data_core.rh,DIST_FILES_ENGINEDATA_BASE_CORE))
$(eval $(call win32-resource-embed-macro,dists/scummvm_rc_engine_data.rh,DIST_FILES_ENGINEDATA_BASE))
$(eval $(call win32-resource-embed-macro,dists/scummvm_rc_engine_data_big.rh,DIST_FILES_ENGINEDATA_BASE_BIG))

# Special target to create a win32 snapshot binary (for Inno Setup)
win32-data: all
	mkdir -p $(WIN32PATH)
	mkdir -p $(WIN32PATH)/doc
	mkdir -p $(WIN32PATH)/doc/cz
	mkdir -p $(WIN32PATH)/doc/da
	mkdir -p $(WIN32PATH)/doc/de
	mkdir -p $(WIN32PATH)/doc/es
	mkdir -p $(WIN32PATH)/doc/fr
	mkdir -p $(WIN32PATH)/doc/it
	mkdir -p $(WIN32PATH)/doc/ko
	mkdir -p $(WIN32PATH)/doc/no-nb
	mkdir -p $(WIN32PATH)/doc/sv
	$(STRIP) $(EXECUTABLE) -o $(WIN32PATH)/$(EXECUTABLE)
	cp $(srcdir)/AUTHORS $(WIN32PATH)/AUTHORS.txt
	cp $(srcdir)/COPYING $(WIN32PATH)/COPYING.txt
	cp $(srcdir)/LICENSES/COPYING.Apache $(WIN32PATH)/COPYING.Apache.txt
	cp $(srcdir)/LICENSES/COPYING.BSD $(WIN32PATH)/COPYING.BSD.txt
	cp $(srcdir)/LICENSES/COPYING.BSL $(WIN32PATH)/COPYING.BSL.txt
	cp $(srcdir)/LICENSES/COPYING.FREEFONT $(WIN32PATH)/COPYING.FREEFONT.txt
	cp $(srcdir)/LICENSES/COPYING.GLAD $(WIN32PATH)/COPYING.GLAD.txt
	cp $(srcdir)/LICENSES/COPYING.LGPL $(WIN32PATH)/COPYING.LGPL.txt
	cp $(srcdir)/LICENSES/COPYING.LUA $(WIN32PATH)/COPYING.LUA.txt
	cp $(srcdir)/LICENSES/COPYING.MIT $(WIN32PATH)/COPYING.MIT.txt
	cp $(srcdir)/LICENSES/COPYING.MKV $(WIN32PATH)/COPYING.MKV.txt
	cp $(srcdir)/LICENSES/COPYING.MPL $(WIN32PATH)/COPYING.MPL.txt
	cp $(srcdir)/LICENSES/COPYING.OFL $(WIN32PATH)/COPYING.OFL.txt
	cp $(srcdir)/LICENSES/COPYING.ISC $(WIN32PATH)/COPYING.ISC.txt
	cp $(srcdir)/LICENSES/COPYING.TINYGL $(WIN32PATH)/COPYING.TINYGL.txt
	cp $(srcdir)/LICENSES/CatharonLicense.txt $(WIN32PATH)/CatharonLicense.txt
	cp $(srcdir)/COPYRIGHT $(WIN32PATH)/COPYRIGHT.txt
	cp $(srcdir)/doc/cz/PrectiMe $(WIN32PATH)/doc/cz/PrectiMe.txt
	cp $(srcdir)/doc/QuickStart $(WIN32PATH)/doc/QuickStart.txt
	cp $(srcdir)/doc/es/InicioRapido $(WIN32PATH)/doc/es/InicioRapido.txt
	cp $(srcdir)/doc/fr/DemarrageRapide $(WIN32PATH)/doc/fr/DemarrageRapide.txt
	cp $(srcdir)/doc/it/GuidaRapida $(WIN32PATH)/doc/it/GuidaRapida.txt
	cp $(srcdir)/doc/ko/QuickStart $(WIN32PATH)/doc/ko/QuickStart.txt
	cp $(srcdir)/doc/no-nb/HurtigStart $(WIN32PATH)/doc/no-nb/HurtigStart.txt
	cp $(srcdir)/doc/da/HurtigStart $(WIN32PATH)/doc/da/HurtigStart.txt
	cp $(srcdir)/doc/de/Schnellstart $(WIN32PATH)/doc/de/Schnellstart.txt
	cp $(srcdir)/doc/sv/Snabbstart $(WIN32PATH)/doc/sv/Snabbstart.txt
ifdef USE_PANDOC
	cp NEWS$(PANDOCEXT) $(WIN32PATH)/NEWS.txt
	cp README$(PANDOCEXT) $(WIN32PATH)/README.txt
	cp doc/de/NEUES$(PANDOCEXT) $(WIN32PATH)/doc/de/NEUES.txt
else
	cp $(srcdir)/NEWS.md $(WIN32PATH)/NEWS.txt
	cp $(srcdir)/README.md $(WIN32PATH)/README.txt
	cp $(srcdir)/doc/de/NEUES.md $(WIN32PATH)/doc/de/NEUES.txt
endif
	cp $(srcdir)/doc/de/LIESMICH $(WIN32PATH)/doc/de/LIESMICH.txt
	cp $(srcdir)/doc/sv/LasMig $(WIN32PATH)/doc/sv/LasMig.txt
	unix2dos $(WIN32PATH)/*.txt
	unix2dos $(WIN32PATH)/doc/*.txt
	unix2dos $(WIN32PATH)/doc/cz/*.txt
	unix2dos $(WIN32PATH)/doc/da/*.txt
	unix2dos $(WIN32PATH)/doc/de/*.txt
	unix2dos $(WIN32PATH)/doc/es/*.txt
	unix2dos $(WIN32PATH)/doc/fr/*.txt
	unix2dos $(WIN32PATH)/doc/it/*.txt
	unix2dos $(WIN32PATH)/doc/no-nb/*.txt
	unix2dos $(WIN32PATH)/doc/sv/*.txt

win32dist: win32-data
	mkdir -p $(WIN32PATH)/graphics
	cp $(srcdir)/dists/win32/graphics/left.bmp $(WIN32PATH)/graphics
	cp $(srcdir)/dists/win32/graphics/scummvm-install.ico $(WIN32PATH)/graphics
	cp $(srcdir)/dists/win32/graphics/scummvm-install.bmp $(WIN32PATH)/graphics
	cp $(srcdir)/dists/win32/ScummVM.iss $(WIN32PATH)
ifdef WIN32SDLDOCPATH
	cp $(WIN32SDLDOCPATH)/README-SDL.txt $(WIN32PATH)/README-SDL.txt
endif
ifdef WIN32SDLPATH
	cp $(WIN32SDLPATH)/SDL2.dll $(WIN32PATH)
ifdef USE_SDL_NET
	cp $(WIN32SDLPATH)/SDL2_net.dll $(WIN32PATH)
	sed -e '/SDL2_net\.dll/ s/^;//' -i $(WIN32PATH)/ScummVM.iss
endif
endif
ifdef WIN32SPARKLEPATH
ifdef USE_SPARKLE
	cp $(WIN32SPARKLEPATH)/WinSparkle.dll $(WIN32PATH)
	sed -e '/WinSparkle\.dll/ s/^;//' -i $(WIN32PATH)/ScummVM.iss
endif
endif

win32dist-mingw: win32-data
ifneq (,$(findstring peldd,$(LDD)))
	$(LDD) $(WIN32PATH)/$(EXECUTABLE) | xargs -I files cp -vu files $(WIN32PATH)
else
	ldd $(WIN32PATH)/$(EXECUTABLE) | grep -i mingw | cut -d">" -f2 | cut -d" " -f2 | sort -u | xargs -I files cp -vu files $(WIN32PATH)
endif

.PHONY: win32-data win32dist win32dist-mingw

include $(srcdir)/ports.mk
