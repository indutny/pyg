#ifndef SRC_PYG_H_
#define SRC_PYG_H_

#include "src/common.h"
#include "src/queue.h"

#include "parson.h"

/* Forward declarations */
struct pyg_gen_s;

typedef struct pyg_s pyg_t;
typedef struct pyg_state_s pyg_state_t;
typedef struct pyg_target_s pyg_target_t;
typedef struct pyg_source_s pyg_source_t;
typedef struct pyg_settings_s pyg_settings_t;

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
  struct {
    pyg_hashmap_t map;
    QUEUE list;
  } children;

  struct {
    pyg_hashmap_t map;
    QUEUE list;
  } target;

  QUEUE member;
};

enum pyg_target_type_e {
  kPygTargetExecutable,
  kPygTargetStatic,
  kPygTargetShared,

  kPygTargetDefaultType = kPygTargetExecutable
};
typedef enum pyg_target_type_e pyg_target_type_t;

struct pyg_target_s {
  pyg_t* pyg;
  JSON_Object* json;

  const char* name;
  pyg_target_type_t type;

  QUEUE member;

  struct {
    pyg_target_t** list;
    unsigned int count;
  } deps;

  struct {
    pyg_source_t* list;
    unsigned int count;
  } source;
};

enum pyg_source_type_e {
  kPygSourceC,
  kPygSourceCC,
  kPygSourceObjC,
  kPygSourceObjCC,

  /* .o, .so, .dylib and other linkable, but non-compilable stuff */
  kPygSourceLink,

  /* Headers and various non-compilable stuff */
  kPygSourceSkip
};
typedef enum pyg_source_type_e pyg_source_type_t;

struct pyg_source_s {
  pyg_source_type_t type;
  const char* path;
  char* out;
};

struct pyg_settings_s {
  const char* builddir;

  struct pyg_gen_s* gen;
  pyg_buf_t* out;
};

pyg_error_t pyg_new(const char* path, pyg_t** out);
void pyg_free(pyg_t* pyg);

pyg_error_t pyg_translate(pyg_t* pyg, pyg_settings_t* settings);

#endif  /* SRC_PYG_H_ */
