# The lime* project

The lime project aims to provide a simple yet extensive and modular programming language and environment. 
At its core lime is a virtual machine written in C as well as a concatenative programming language which runs on it.

The first version `v0.0.0-alpha` offers the most basic architecture, including a foreign function interface using shared libraries and dynamic linking.
To allow a modular development from the ground up, this project only implements an internal API. 
The actual instructions of the concatenative programming language are exposed by the [lime-kernel](https://github.com/kuchenkruste/lime-kernel) project using the foreign function interface.

> \* The project's name is only temporarily and may change in the near future. For now it is intended to be a pun, hinting at the software's architecture whose modular components can be glued together very freely.

# Contributing

Contributions of any kind (of course in form of pull requests) are more than welcome. Please do not hesitate to make use of the issue system as well.

Please note, however, that this project is the virtual machine's core. To contribute functionality to the concatenative programming language refer to one of the following repositories:

* [lime-kernel](https://github.com/kuchenkruste/lime-kernel)
* [lime-io](https://github.com/kuchenkruste/lime-io)

Extensions to this project, which are meant to be part of the public C API, must be exposed in the interface headers of the [lime-api](https://github.com/kuchenkruste/lime-api) project.
