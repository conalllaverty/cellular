IMPORTANT: this API is still in the process of definition, anything may change, beware!  We aim to have it settled by the middle of 2020, COVID 19 permitting.

# Introduction
These directories provide the porting layer that allows the files in the `ctrl` and `sock` directories to be linked and run on a target platform.

No attempt is made to create a full platform porting API; only the APIs necessay for `ctrl` and `sock` to run are implemented here.

# Usage
The `api` directory defines the platform API, as required by `ctrl` and `sock`.  The `clib` directory contains a generic mapping of the C library portion of the porting API to the standard C library; this can usually be employed on any platform.  In the `platform` directory you will find the implementation of the remainder of the porting API on various target platforms and the necessary instructions to create a working binary and tests on each of those target platforms.  Please refer to the `README.md` file in your chosen target platform directory for more information.

The `test` directory contains tests for the porting layer that can be run on any platform.