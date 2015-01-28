#ifndef SRC_PYG_INTERNAL_H_
#define SRC_PYG_INTERNAL_H_

pyg_error_t pyg_add_var(pyg_t* pyg,
                        pyg_hashmap_t* vars,
                        const char* key,
                        const char* value);

#endif  /* SRC_PYG_INTERNAL_H_ */
