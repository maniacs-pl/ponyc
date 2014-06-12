#include "traits.h"
#include "subtype.h"
#include "reify.h"
#include "typechecker.h"
#include <assert.h>

typedef enum
{
  TYPE_INITIAL = 0,
  TYPE_TRAITS_IN_PROGRESS,
  TYPE_TRAITS_DONE
} type_state_t;

static bool attach_method(ast_t* type, ast_t* method)
{
  ast_t* members = ast_childidx(type, 4);

  // see if we have an existing method with this name
  const char* name = ast_name(ast_childidx(method, 1));
  ast_t* existing = ast_get(type, name);

  if(existing != NULL)
  {
    // check our version is a subtype of the supplied version
    if(!is_subtype(existing, method))
    {
      ast_error(existing, "existing method is not a subtype of trait method");
      ast_error(method, "trait method is here");
      return false;
    }

    // TODO: if existing has no implementation, accept this implementation
    // what if a new method is a subtype of an existing method?
    // if the existing method came from a trait, should we accept the new one?

    return true;
  }

  // insert into our members
  ast_t* r_method = ast_dup(method);
  ast_append(members, r_method);

  // add to our scope
  ast_set(type, name, r_method);

  return true;
}

static bool attach_traits(ast_t* def)
{
  type_state_t state = (type_state_t)ast_data(def);

  switch(state)
  {
    case TYPE_INITIAL:
      ast_attach(def, (void*)TYPE_TRAITS_IN_PROGRESS);
      break;

    case TYPE_TRAITS_IN_PROGRESS:
      ast_error(def, "traits cannot be recursive");
      return false;

    case TYPE_TRAITS_DONE:
      return true;
  }

  ast_t* traits = ast_childidx(def, 3);
  ast_t* trait = ast_child(traits);

  while(trait != NULL)
  {
    ast_t* nominal = ast_child(trait);

    if(ast_id(nominal) != TK_NOMINAL)
    {
      ast_error(nominal, "traits must be nominal types");
      return false;
    }

    ast_t* trait_def = nominal_def(nominal);

    if(ast_id(trait_def) != TK_TRAIT)
    {
      ast_error(nominal, "must be a trait");
      return false;
    }

    if(!attach_traits(trait_def))
      return false;

    ast_t* typeparams = ast_childidx(trait_def, 1);
    ast_t* typeargs = ast_childidx(nominal, 2);
    ast_t* members = ast_childidx(trait_def, 4);
    ast_t* member = ast_child(members);

    while(member != NULL)
    {
      switch(ast_id(member))
      {
        case TK_NEW:
        case TK_FUN:
        case TK_BE:
        {
          // reify the method with the type parameters from trait_def
          // and the reified type arguments from r_trait
          ast_t* r_member = reify(member, typeparams, typeargs);
          bool ok = attach_method(def, r_member);

          if(r_member != member)
            ast_free(r_member);

          if(!ok)
            return false;

          break;
        }

        default: assert(0);
      }

      member = ast_sibling(member);
    }

    trait = ast_sibling(trait);
  }

  ast_attach(def, (void*)TYPE_TRAITS_DONE);
  return true;
}

bool type_traits(ast_t* ast, int verbose)
{
  switch(ast_id(ast))
  {
    case TK_TRAIT:
    case TK_CLASS:
    case TK_ACTOR:
      return attach_traits(ast);

    default: {}
  }

  return true;
}
