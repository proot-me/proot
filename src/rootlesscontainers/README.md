# rootlesscontainers.proto

This directory contains `rootlesscontainers.proto`, which is used for preserving emulated file owner information as `user.rootlesscontainers` xattr values.

## Source

https://raw.githubusercontent.com/rootless-containers/rootlesscontaine.rs/f7b0f5486bb0e0be75771ce50c54471296f040eb/docs/proto/rootlesscontainers.proto

## Compile

```console
$ protoc-c --c_out=. rootlesscontainers.proto
```
