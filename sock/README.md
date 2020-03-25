IMPORTANT: this API is still in the process of definition, anything may change, beware!  We aim to have it settled by the middle of 2020, COVID 19 permitting.

# Introduction
These directories provide a driver that implements the user data (sockets) interface to a cellular module.  The API presented is a Berkeley sockets API, similar to that of LWIP, allowing the files here to replace LWIP, providing a cellular-based sockets interface through the IP stack inside the cellular module instead. 

The files under the `ctrl` directory provide the actual AT interface to the cellular module and hence are required by this driver (and those of `port`, see next section) to achieve a usable binary image.

# Usage
The directories include only the API and pure C source files that make no reference to a platform, a C library or an operating system.  They rely upon the `port` directory to map to a target platform and provide the necessary build/test infrastructure for that target platform; see the relevant platform directory under `port` for build and usage information.

# Testing
The `test` directory contains generic tests for the `sock` API. Please refer to the relevant platform directory of the `port` component for instructions on how to build and run the tests.