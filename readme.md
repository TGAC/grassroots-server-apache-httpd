# Apache httpd module

This module allows the Grassroots infrastructure to be used with an [Apache httpd](http://httpd.apache.org) web server. This means that the Grassroots infrastructure can take advantage of all of the functionality that httpd provides.

## Installation

To build this module, you need the [grassroots core](https://github.com/TGAC/grassroots-core) and [grassroots build config](https://github.com/TGAC/grassroots-build-config) installed and configured. 

The files to build the Grassroots module are in the ```build/<platform>``` directory. 

### Linux

If you enter this directory 

```cd build/linux```

you can then build the service by typing

```make all```

and then 

```make install```

to install the module into the appropriate httpd installation allowing the Grassroots system to be used in conjunction with it.

## Configuration

As well as installing the module, httpd requires a couple of alterations to allow the Grassroots system to be available.

### Module configuration file

The first change is to set up the module configuration. This is done using a standard httpd configuration file and an example is at ```conf/httpd_grassroots.conf``` in this repository.
It requires some modules to provide the caching functionality to store job data between requests in a multi-process and multi-threaded environment

The only parts that a server administrator would need to change are what uri to use for the Grassroots system and the directory in which the Grassroots system is installed.

The uri is set using the standard httpd directives such as ```<Location>``` and ```<LocationMatch>``` and the installation directory is specified using the ```GrassrootsRoot``` directive which takes a string denoting the path. The content of an example configuration is file is shown below.

~~~{conf}
#
# Load the Grassroots module and the caching modules required
# for storing persistent data when the server is running
#
LoadModule grassroots_module modules/mod_grassroots.so
LoadModule cache_module modules/mod_cache.so
LoadModule cache_socache_module modules/mod_cache_socache.so
LoadModule socache_shmcb_module modules/mod_socache_shmcb.so

#
# Set the caching preferences for storing the persistent data
#
CacheSocache shmcb
CacheSocacheMaxSize 102400

#
# Set the uri for the Grassroots infrastructure requests
#
<LocationMatch "/grassroots/controller">
	
	# Let Grassroots handle these requests
	SetHandler grassroots-handler
	
	# The path to the Grassroots root directory 
	GrassrootsRoot /opt/grassroots-0/grassroots
</LocationMatch>
~~~

### Updating envvars

The Apache httpd server also needs to have access to the paths where the Grassroots infrastructure and its dependencies are located. This is done be editing the ```envvars``` file that is part of the httpd distribution and is normally located at ```bin/envvars```.

~~~{properties}
# Begin Grassroots

# Set the Grassroots installation path
GRASSROOTS_PATH=/opt/grassroots-0/grassroots

# Add all of the paths for the dependencies
LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$GRASSROOTS_PATH/lib"
LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$GRASSROOTS_PATH/extras/mongodb-c/lib"
LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$GRASSROOTS_PATH/extras/samtools/lib"
LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$GRASSROOTS_PATH/extras/htmlcxx/lib"
LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$GRASSROOTS_PATH/extras/hcxselect/lib"
LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$GRASSROOTS_PATH/extras/irods/lib"
LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$GRASSROOTS_PATH/extras/slurm_drmaa/lib"
LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$GRASSROOTS_PATH/extras/jansson/lib"
LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$GRASSROOTS_PATH/extras/blast/lib"
LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$GRASSROOTS_PATH/extras/libuuid/lib"
LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$GRASSROOTS_PATH/extras/dropbox-c/lib"
LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$GRASSROOTS_PATH/extras/liboauth/lib"
LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$GRASSROOTS_PATH/extras/pcre/lib"

# End Grassroots
~~~
