#ifndef SRC_PYG_H_
#define SRC_PYG_H_

#include "src/common.h"

#include "parson.h"

/* Forward declarations */
struct pyg_gen_s;

typedef struct pyg_s pyg_t;
typedef struct pyg_state_s pyg_state_t;
typedef struct pyg_target_s pyg_target_t;

struct pyg_s {
  /* 0 - for root, > 0 for child */
  unsigned int id;
  unsigned int child_count;

  JSON_Value* json;
  JSON_Object* obj;
  char* path;
  char* dir;

  pyg_t* root;
  pyg_t* parent;
  pyg_hashmap_t children;

  pyg_hashmap_t targets;
};

struct pyg_state_s {
  struct pyg_s* pyg;
  struct pyg_gen_s* gen;
  struct pyg_buf_s* buf;
};

struct pyg_target_s {
  const char* name;
  const char* type;
};

pyg_error_t pyg_new(const char* path, pyg_t** out);
void pyg_free(pyg_t* pyg);

pyg_error_t pyg_load(pyg_t* pyg);
pyg_error_t pyg_translate(pyg_t* pyg, struct pyg_gen_s* gen, pyg_buf_t* buf);

#endif  /* SRC_PYG_H_ */
