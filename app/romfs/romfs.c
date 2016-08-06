/*
 * (C) Copyright 2013-16 Andy Green <andy@warmcat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 * all of these members are big-endian
 */

#include <romfs.h>

struct sb {
	unsigned int be_magic1;
	unsigned int be_magic2;
	unsigned int be_size;
	unsigned int be_checksum;
	char name[0];
};

struct inode {
	unsigned int be_next;
	unsigned int be_spec; /* DIR: offset from romfs start to first entry */
	unsigned int be_size;
	unsigned int be_checksum;
	char name[0];
};

/* these magics are already in big-endian and don't need converting */
#define BE_ROMFS_MAGIC1 0x6d6f722d
#define BE_ROMFS_MAGIC2 0x2d736631

extern void
memcpy_aligned(uint32_t *dst32, const uint32_t *src32, int len);

static uint32_t
deref32(const uint32_t *p)
{
	return *((uint32_t *)p);
}

static
unsigned char deref8(const unsigned char *p)
{
	uint32_t v = *((uint32_t *)((long)(void *)p & ~3));
	
	switch (((int)(long)(void *)p) & 3) {
	case 0:
		return v;
	case 1:
		return v >> 8;
	case 2:
		return v >> 16;
	default:
		break;
	}
	
	return v >> 24;
}

static unsigned int
le(const unsigned int *be)
{
	uint32_t v = deref32(be);
	const unsigned char *c = (const unsigned char *)&v;

	return (c[0] << 24) | (c[1] << 16) | (c[2] << 8) | c[3];
}

/* rule is pad strings to 16 byte boundary */
static int
pad(void *s1)
{
	const char *s = s1;
	int n = 0;

	while (deref8(s++))
		n++;

	return (n + 0xf) & ~0xf;
}

size_t
romfs_inode_size(const struct inode *inode)
{
	return le(&inode->be_size);
}

const struct inode *
romfs_lookup(const void *sb, const void *start, const char *filepath)
{
	const struct inode *inode = (struct inode *)
			((char *)sb + sizeof(struct sb) + pad(((struct sb *)sb)->name));
	const struct inode *level = inode;
	const char *target;
	const char *p, *n;
	int m;

	if (start != (void *)sb) {
		inode = start;
		level = start;
	} else
		while (*filepath == '/')
			filepath++;

	while (inode != sb) { /* this indicates reached end of files */
		p = filepath;
		n = &inode->name[0];

		while (p && *p != '/' && deref8(n) && *p == deref8(n)) {
			p++;
			n++;
		}

		/* we found the whole path? */

		if (!*p && !deref8(n)) {
			m = le(&inode->be_next) & 7;
			switch (m) {
			case 0: /* hard link */
				return (struct inode *)
					((char *)sb + (le(&inode->be_spec) & ~0xf));
			case 3: /* symlink */
				target = ((char *)(inode + 1)) +
							       pad(&inode->name);
				if (deref8(target) == '/') {
					/* reinterpret symlink path from / */
					level = (struct inode *)
						((char *)sb + sizeof(struct sb) +
								 pad(&((struct sb *)sb)->name));
					target++;
				} /* else reinterpret from cwd */
				inode = romfs_lookup(sb, level, target);
				continue;
			default: /* file of some kind, or dir */
				return inode;
			}
		}

		/* we matched a dir part */
		if (*p == '/' && !deref8(n)) {

			m = le(&inode->be_next) & 7;
			switch (m) {
			case 0: /* hard link */
				return (struct inode *)
					((char *)sb + (le(&inode->be_spec) & ~0xf));
			case 3: /* symlink */
				target = ((char *)(inode + 1)) +
							       pad(&inode->name);
				if (deref8(target) == '/') {
					/* reinterpret symlink path from / */
					level = (struct inode *)
						((char *)sb + sizeof(struct sb) +
								 pad(&((struct sb *)sb)->name));
					target++;
				} /* else reinterpret from cwd */
				inode = romfs_lookup(sb, level, target);
				if (!inode)
					return 0;
				/*
				 * resume looking one level deeper
				 * directories have special "first entry"
				 * offset that accounts for padding
 				 */
				inode = (struct inode *)((char *)sb + le(&inode->be_spec));

				while (*filepath != '/' && *filepath)
					filepath++;
				if (!*filepath)
					return 0;
				filepath++;
				continue;

			default: /* file of some kind, or dir */
				/* move past the / */
				filepath = p + 1;
				if (m == 1) /* dir */
					/* use special ofs to skip padding */
					inode = (struct inode *)((char *)sb + le(&inode->be_spec));
				else
					/* resume looking one level deeper */
					inode = (struct inode *)
						(((char *)(inode + 1)) +
						pad(&inode->name));
				break;
			}
			level = inode;
			continue;
		}

		/* not a match, try the next at this level */

		if (!(le(&inode->be_next) & ~0xf))
			/* no more at this level */
			return 0;

		inode = (struct inode *)((char *)sb + (le(&inode->be_next) & ~0xf));
	}

	return 0;
}

size_t
romfs_read(const struct inode *inode, size_t ofs, uint32_t *buf, size_t maxsize)
{
	size_t len = le(&inode->be_size) - ofs;
	
	if (len > maxsize)
		len = maxsize;

	memcpy_aligned(buf, (const uint32_t *)(inode->name + pad(&inode->name) + ofs), len);

	return len;
}
