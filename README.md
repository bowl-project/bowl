# The bowl project

The bowl project aims to provide a simple yet extensive and modular programming platform. 
In the near future, bowl is intended to be used as an extensive shell language.
At its core, bowl is a virtual machine written in C as well as a concatenative programming language, which runs on the virtual machine.

The first version `v0.0.0-alpha` offers the most basic architecture, including a foreign function interface using shared libraries and dynamic linking.
To allow a modular development from the ground up, this project only implements an internal API. 
The actual instructions of the concatenative programming language are exposed by the [bowl-kernel](https://github.com/kuchenkruste/bowl-kernel) project using the foreign function interface.

# Contributing

Contributions of any kind are more than welcome. Please do not hesitate to make use of the issue system as well.

Please note, however, that this project is the virtual machine's core. To contribute functionality to the concatenative programming language refer to one of the following repositories:

* [bowl-kernel](https://github.com/kuchenkruste/bowl-kernel)
* [bowl-io](https://github.com/kuchenkruste/bowl-io)

Extensions to this project, which are meant to be part of the public C API, must be exposed in the interface headers of the [bowl-api](https://github.com/kuchenkruste/bowl-api) project.
