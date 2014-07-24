#ifndef __ION_HELPER_LIB_H__
#define __ION_HELPER_LIB_H__

#ifdef __cplusplus
extern "C"
{
#endif

struct ion_args *ion_malloc(int size, int attrs);
int ion_free(struct ion_args *args);
void ion_flush_cache(struct ion_args *ion_args, int offset, int size, int dir);

#ifdef __cplusplus
}
#endif

#endif /* __ION_HELPER_LIB_H__ */
