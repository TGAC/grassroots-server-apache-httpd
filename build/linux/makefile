NAME 		= grassroots
DIR_BUILD :=  $(realpath $(dir $(lastword $(MAKEFILE_LIST))))
DIR_SRC := $(realpath $(DIR_BUILD)/../../src)
DIR_INCLUDE := $(realpath $(DIR_BUILD)/../../include)

ifeq ($(DIR_BUILD_CONFIG),)
export DIR_BUILD_CONFIG := $(realpath $(DIR_BUILD)/../../../../build-config/linux)
endif

include $(DIR_BUILD_CONFIG)/project.properties

-include $(DIR_BUILD)/apache_server_module.properties

ifeq ($(DIR_APACHE),)
DIR_APACHE_MODULES := /usr/lib/apache2/modules
DIR_APACHE_INC := 
DIR_APACHE_CONF = /etc/apache2/mods-enabled
DIR_APACHE_ENVVARS := /etc/apache2/
APACHE_SET = true
APXS = apxs
MODULE_LOAD_FILE := ../../conf/load_grassroots_for_packaged_apache.conf
else
DIR_APACHE_MODULES := $(DIR_APACHE)/modules
DIR_APACHE_INC := $(DIR_APACHE)/include
DIR_APACHE_CONF = $(DIR_APACHE)/conf/extra
DIR_APACHE_ENVVARS := $(DIR_APACHE)/bin
MODULE_LOAD_FILE := ../../conf/load_grassroots_for_compiled_apache.conf
APXS = $(DIR_APACHE)/bin/apxs
endif


ifeq ($(DIR_ROOT),)
export DIR_ROOT = $(realpath $(../../..))
endif

CFLAGS 	+=  -Wc,-Wall  -Wc,-fpic

ifeq ($(BUILD),release)
	CFLAGS 	+= -Wc,-O3 -Wc,-s
 	LDFLAGS += Wl,-s
else
	CFLAGS 	+= -Wc,-g -Wc,-Wall -Wc,-O0
	LDFLAGS += -Wl,pg
	CPPFLAGS += -Wc,-D_DEBUG
endif


ESCAPED_DIR_GRASSROOTS_INSTALL := $(shell echo "$(DIR_GRASSROOTS_INSTALL)" | sed 's/\//\\\//g')
ESCAPED_DIR_GRASSROOTS_EXTRAS := $(shell echo "$(DIR_GRASSROOTS_EXTRAS)" | sed 's/\//\\\//g')


VPATH	:= \
	$(DIR_SRC) \
	$(DIR_GRASSROOTS_SERVER_SRC) 	


ifneq ($(DIR_APACHE_INC),)
INCLUDES = -I$(DIR_APACHE_INC)
endif


INCLUDES += \
	-I$(DIR_INCLUDE) \
	-I$(DIR_GRASSROOTS_SERVER_INC) \
	-I$(DIR_GRASSROOTS_UUID_INC) \
	-I$(DIR_GRASSROOTS_UTIL_INC) \
	-I$(DIR_GRASSROOTS_UTIL_INC)/containers \
	-I$(DIR_GRASSROOTS_UTIL_INC)/io \
	-I$(DIR_GRASSROOTS_NETWORK_INC) \
	-I$(DIR_GRASSROOTS_IRODS_INC) \
	-I$(DIR_GRASSROOTS_SERVICES_INC) \
	-I$(DIR_GRASSROOTS_SERVER_INC) \
	-I$(DIR_GRASSROOTS_PARAMS_INC) \
	-I$(DIR_GRASSROOTS_HANDLER_INC) \
	-I$(DIR_GRASSROOTS_TASK_INC) \
	-I$(DIR_JANSSON_INC) \
	-I$(DIR_UUID_INC)	
		
SRCS 	= \
	$(DIR_SRC)/mod_grassroots.c \
	$(DIR_SRC)/key_value_pair.c \
	$(DIR_SRC)/apr_global_storage.c \
	$(DIR_SRC)/apr_jobs_manager.c \
	$(DIR_SRC)/apr_external_servers_manager.c \
	$(DIR_SRC)/apache_output_stream.c \
	$(DIR_SRC)/apr_grassroots_servers.c

LDFLAGS += -L$(DIR_GRASSROOTS_UTIL_LIB) -l$(GRASSROOTS_UTIL_LIB_NAME) \
	-L$(DIR_GRASSROOTS_SERVICES_LIB) -l$(GRASSROOTS_SERVICES_LIB_NAME) \
	-L$(DIR_GRASSROOTS_HANDLER_LIB) -l$(GRASSROOTS_HANDLER_LIB_NAME) \
	-L$(DIR_GRASSROOTS_NETWORK_LIB) -l$(GRASSROOTS_NETWORK_LIB_NAME) \
	-L$(DIR_GRASSROOTS_SERVER_LIB) -l$(GRASSROOTS_SERVER_LIB_NAME) \
	-L$(DIR_GRASSROOTS_UUID_LIB) -l$(GRASSROOTS_UUID_LIB_NAME) \
	-L$(DIR_GRASSROOTS_TASK_LIB) -l$(GRASSROOTS_TASK_LIB_NAME) 


ifeq ($(USE_COMPRESSION),bzip2)
SRCS 	+= $(DIR_SRC)/bzip2_util.c
CFLAGS += -DUSE_BZIP2
LDFLAGS += -L$(DIR_BZIP2_LIB) -l$(BZIP2_LIB_NAME) 
INCLUDES += -I$(DIR_BZIP2_INC)	
endif

 
# Compile and generate dependency info
# 1. Compile the .c file
# 2. Generate dependency information, explicitly specifying the target name
$(DIR_OBJS)/%.o : %.c
	@echo ">>> c build for $@"
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ -c $<
	$(MAKEDEPEND) $(basename $@).d -MT $(basename $@).o $(CPPFLAGS) $(CFLAGS) $<  
	@mv -f $(basename $@).d $(basename $@).d.tmp
	@sed -e 's|.*:|$*.o:|' < $(basename $@).d.tmp > $(basename $@).d
	@sed -e 's/.*://' -e 's/\\$$//' < $(basename $@).d.tmp | fmt -1 | \
		sed -e 's/^ *//' -e 's/$$/:/' >> $(basename $@).d
	@rm -f $(basename $@).d.tmp   	

.PHONY:	all env conf


env:
	@echo "v $(v)" 
	@echo "HTTPD $(HTTPD)" 
	@echo "APACHE SET $(APACHE_SET)"
	@echo "DIR_APACHE _$(DIR_APACHE) _"
	@echo "APACHE_HOME  _$$APACHE_HOME_"
	@echo "DIR_APACHE_MODULES _$(DIR_APACHE_MODULES)_"
	@echo "DIR_APACHE_CONF _$(DIR_APACHE_CONF)_"
	@echo "APXS $(APXS)"
	@echo "USE_COMPRESSION $(USE_COMPRESSION)"

all:
	$(APXS) -n $(NAME) $(INCLUDES) -D UNIX=1 -D BUILD=$(BUILD) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -c $(SRCS)
		
clean:
	@rm -fr src/*.o src/*.lo src/*.slo src/*.la
		
install: all conf envvars
	@echo "Installing mod_$(NAME).so to $(DIR_APACHE_MODULES)"
	$(SUDO) cp $(DIR_SRC)/.libs/mod_$(NAME).so $(DIR_APACHE_MODULES)
	#if [ -d $(DIR_APACHE)/conf/conf.d ]; then $(DIR_APACHE_CONF)=$(DIR_APACHE)/conf/conf.d; fi
	if [ ! -e $(DIR_APACHE_CONF)/grassroots.conf ]; then \
		$(SUDO) cp ../../conf/grassroots.conf $(DIR_APACHE_CONF); \
	fi

conf:
	if [ ! -e ../../conf/grassroots.conf ]; then \
		cat $(MODULE_LOAD_FILE) > ../../conf/grassroots.conf; \
		cat ../../conf/httpd-grassroots.conf >> ../../conf/grassroots.conf; \
		sed -i "s/DIR_GRASSROOTS_INSTALL/$(ESCAPED_DIR_GRASSROOTS_INSTALL)/g" ../../conf/grassroots.conf; \
	fi
	

envvars:
	if [ ! -e  $(DIR_APACHE_ENVVARS)/envvars_grassroots ]; then \
		cp ../../conf/envvars ../../conf/envvars_new; \
		sed -i "s/DIR_GRASSROOTS_EXTRAS/$(ESCAPED_DIR_GRASSROOTS_EXTRAS)/g" ../../conf/envvars_new; \
		sed -i "s/DIR_GRASSROOTS/$(ESCAPED_DIR_GRASSROOTS_INSTALL)/g" ../../conf/envvars_new; \
		$(SUDO) cp ../../conf/envvars_new $(DIR_APACHE_ENVVARS)/envvars_grassroots; \
		$(SUDO) sed -i -e '$$a\' -e '\n. $(DIR_APACHE_ENVVARS)/envvars_grassroots\n' $(DIR_APACHE_ENVVARS)/envvars; \
	fi

