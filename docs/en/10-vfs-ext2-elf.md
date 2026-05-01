# VFS, EXT2 and ELF Loader

[PL](../10-vfs-ext2-i-elf.md) | [EN](./10-vfs-ext2-elf.md)

VFS is the unified file abstraction for process launch and runtime I/O.

Key points:

- `vfs_find` resolves path to node,
- `vfs_read` provides file data for ELF parsing/loading,
- EXT2 mount extends persistent storage backend,
- ELF loader maps segments and prepares process entry.
