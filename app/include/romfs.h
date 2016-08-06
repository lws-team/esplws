#include <c_stddef.h>
#include <c_types.h>

struct inode;

const struct inode *
romfs_lookup(const void *sb, const void *start, const char *filepath);

size_t
romfs_read(const struct inode *inode, size_t ofs, uint32_t *buf, size_t maxsize);

size_t
romfs_inode_size(const struct inode *inode);

