# Apache httpd module

This module allows the Grassroots infrastructure to be used with an 
[Apache httpd](http://httpd.apache.org) web server. This means that the Grassroots 
infrastructure can take advantage of all of the functionality that httpd provides.

## Installation

To build this module, you need the [grassroots core](https://github.com/TGAC/grassroots-core)
 and [grassroots build config](https://github.com/TGAC/grassroots-build-config) installed and 
configured. 

The files to build the Grassroots module are in the `build/unix/<platform>` directory. 

### Linux and Mac

The platform build files are in the `unix/<PLATFORM>' directory where platform is either *linux*
or *mac*. If you enter the appropriate directory 

```
cd build/unix/linux
```

or

```
cd build/unix/mac
```


you can then build the service by typing

```
make all
```

and then 

```
make install
```

to install the module into the appropriate httpd installation allowing the Grassroots system to
 be used in conjunction with it.


### Windows

For Windows use the visual studio project in the `build/windows` folder.



## Configuration

As well as installing the module, httpd requires a couple of alterations to 
allow the Grassroots system to be available.

### Module configuration file

The first change is to set up the module configuration. This is done using a standard httpd 
configuration file and an example is at `conf/grassroots.conf` in this repository.
It requires some modules to provide the caching functionality to store job data between requests 
in a multi-process and multi-threaded environment

The only parts that a server administrator would need to change are what uri to use for the 
Grassroots system and the directory in which the Grassroots system is installed.

The uri is set using the standard httpd directives such as `<Location>` and 
`<LocationMatch>` and the installation directory is specified using the `GrassrootsRoot`
 directive which takes a string denoting the path. The content of an example configuration is 
file is shown below.

The available configuration properties are described below:

 * **GrassrootsRoot***: This is required and is the path to the Grassroots installation
 * **GrassrootsConfig**: The config file to use for this Grassroots Server. If omitted, this 
 will default to being *grassroots.config* within the directory specified by the 
 `GrassrootsRoot` directive.
 * **GrassrootsServicesConfigPath**: The path to the individual services config files. If 
 omitted, this will default to being *config* within the directory specified by the
 `GrassrootsRoot` directive.
 * **GrassrootsServicesPath**: The path to the service module files. If 
 omitted, this will default to being *services* within the directory specified by the
 `GrassrootsRoot` directive.
 * **GrassrootsReferenceServicesPath**: The path to the referred service files. If 
 omitted, this will default to being *references* within the directory specified by the
 `GrassrootsRoot` directive.
 * **GrassrootsServersManager**: The path to the Grassroots Servers Manager Module to use. 
 Leave this blank to use the default. 
 * **GrassrootsServersManagersPath**: The path to the service module files. If 
 omitted, this will default to being *servers* within the directory specified by the
 `GrassrootsRoot` directive.
 * **GrassrootsJobManager**: The path to the Grassroots Jobs Manager Module to use
 Leave this blank to use the default. 
 * **GrassrootsJobsManagersPath**: The path to the service module files. If 
 omitted, this will default to being *jobs_managers* within the directory specified by the
 `GrassrootsRoot` directive.


An example file is listed below that specfies that Grassoots is installed in the 
`/opt/grassroots` folder and that it Grassroots will be used for requests to 
*/grassroots/controller*. The global configuration file to be used is `my_config`
and the folder containing the services configuration files is called 
`my_config_files`.

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
# to /grassroots/controller
<LocationMatch "/grassroots/controller">
	
	# Let Grassroots handle these requests
	SetHandler grassroots-handler
	
	# The path to the Grassroots root directory 
	GrassrootsRoot /opt/grassroots

	# The global configuration file to use
	GrassrootsConfig my_config

	# The path to the service configuration files
	GrassrootsServicesConfigPath my_config_files
</LocationMatch>
~~~

### Updating envvars

The Apache httpd server also needs to have access to the paths where the Grassroots 
infrastructure and its dependencies are located. This is done be editing the `envvars` 
file that is part of the httpd distribution and is normally located at `bin/envvars`.

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
