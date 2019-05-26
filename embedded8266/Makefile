include user.cfg


# want COMMITINFO = blank in recursive calls of MAKE
# this produces webpage with git info commits used
ifneq ($(PROJECT_COMMIT_INFO_LINK),)
	COMMITINFO ?= \
	echo "<html><head><title>Git Dirty Info</title><meta charset="UTF-8"></head><body>\\n<PRE>commit " > web/page/commitinfo.html; \
	git rev-parse HEAD  >> web/page/commitinfo.html 2>/dev/null;    \
	git status -uno -v -v |sed -e   's/\&/\&amp;/g' | sed -e  's/</\&lt;/g' |sed -e   's/>/\&gt;/g' >> web/page/commitinfo.html 2>/dev/null;             \
	git submodule status --recursive >> web/page/commitinfo.html 2>/dev/null;             \
	echo "\\nSubmodule esp82xx" >> web/page/commitinfo.html;      \
	(cd esp82xx;git rev-parse HEAD;cd ..) >> web/page/commitinfo.html;   \
	(cd esp82xx;git status -uno -v -v;cd ..)  >> web/page/commitinfo.html;  \
	echo "</PRE>\\n</body></html>" >> web/page/commitinfo.html
else
$(shell rm web/page/commitinfo.html)
endif

ifneq ($(PROJECT_COMMIT_INFO_LINK),)
$(shell $(COMMITINFO))
endif

-include esp82xx/common.mf


#SDK:=/home/cnlohr/esp8266/esp_iot_sdk_v1.5.1
-include esp82xx/main.mf

% :
	$(warning This is the empty rule. Something went wrong.)
	@true

SRCS += user/ws2812_i2s.c \
	user/hpatimer.c \
	user/adc.c \
	../embeddedcommon/DFT32.c \
	../embeddedcommon/embeddednf.c \
	../embeddedcommon/embeddedout.c

LDFLAGS_CORE += -Wl,-Map,output.map

#sed -i "s/{{PROJECT_COMMITINFOLINK}}/" tmp/index.html


ifndef TARGET
$(info Modules were not checked out... use git clone --recursive in the future. Pulling now.)
$(shell git submodule update --init --recursive)
endif

