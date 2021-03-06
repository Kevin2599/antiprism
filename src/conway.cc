/*
   Copyright (c) 2007-2016, Roger Kaufman
   Includes ideas and algorithms by George W. Hart, http://www.georgehart.com

   Antiprism - http://www.antiprism.com

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

      The above copyright notice and this permission notice shall be included
      in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.
*/

/*
   Name: conway.cc
   Description: Conway Notation
                Implementation of George Hart's Conway Notation
                http://www.georgehart.com/virtual-polyhedra/conway_notation.html
   Project: Antiprism - http://www.antiprism.com
*/

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <ctype.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "../base/antiprism.h"

using std::map;
using std::pair;
using std::set;
using std::string;
using std::vector;

using namespace anti;

#define CN_ONE_THIRD 1 / 3.0
#define CN_ONE_HALF 0.5

struct ConwayOperator {
  std::string operator_short;
  std::string operator_name;
  bool allow_n;
  bool hart_operator;
};

// clang-format off
ConwayOperator conway_operator_list[]{
    {"a",  "ambo",            false, true  },
    {"b",  "bevel",           true,  false }, // allows N >= 0
    {"c",  "chamfer",         false, true  },
    {"d",  "dual",            false, true  },
    {"e",  "expand",          true,  false }, // allows N >= 0
    {"g",  "gyro",            false, true  },
    {"J",  "joined-medial",   false, false }, // replaces wiki M0
    {"j",  "join",            false, false },
    {"k",  "kis",             true,  true  }, // allows N >= 3 for vertices
    {"K",  "stake",           true,  false }, // allows N >= 3 for faces
    {"L",  "lace",            true,  false }, // allows N >= 3 for faces, or 0
    {"l",  "loft",            true,  false }, // allows N >= 3 for faces
    {"M",  "medial",          true,  false }, // allows N >= 0
    {"m",  "meta",            true,  false }, // allows N >= 0
    {"n",  "needle",          false, false },
    {"o",  "ortho",           true,  false }, // allows N >= 0
    {"p",  "propellor",       false, true  },
    {"q",  "quinto",          false, false },
    {"r",  "reflect",         false, false },
    {"S",  "seed",            false, false },
    {"s",  "snub",            false, false },
    {"t",  "truncate",        true,  false }, // allows N >= 3 for faces
    {"u",  "subdivide",       false, false },
    {"w",  "whirl",           false, true  },
    {"X",  "cross",           false, false },
    {"z",  "zip",             false, false },
    {"+",  "orient positive", false, false },
    {"-",  "orient negative", false, false },
};
// clang-format on

string operators_str()
{
  string operators;
  int last_op = sizeof(conway_operator_list) / sizeof(conway_operator_list[0]);
  for (int i = 0; i < last_op; i++) {
    operators.push_back(conway_operator_list[i].operator_short[0]);
  }
  return operators;
}

string digits_allowed_str()
{
  string digits_allowed;
  int last_op = sizeof(conway_operator_list) / sizeof(conway_operator_list[0]);
  for (int i = 0; i < last_op; i++) {
    if (conway_operator_list[i].allow_n)
      digits_allowed.push_back(conway_operator_list[i].operator_short[0]);
  }
  return digits_allowed;
}

string uniforms_str() { return "TCOID"; }

string digits_required_str() { return "PAYZR"; }

string operands_str() { return (uniforms_str() + digits_required_str()); }

string digits_ge_3_str() { return "KkLlt"; }

string digits_str() { return "0123456789"; }

class ops {
public:
  int op_pos;
  char op;
  int op_var;
  ops(int n, char o, int v) : op_pos(n), op(o), op_var(v) {}
};

bool cmp_ops(const ops *a, const ops *b) { return a->op_pos > b->op_pos; }

int validate_cn_string(const string &cn_string, vector<ops *> &operations,
                       char &operand, int &poly_size, string alpha_user)
{
  char current_op = '\0';
  string number_string;
  int num_val = 1;
  bool delayed_write = false;

  // get allowable operators from table
  // and get operators which allow N from table
  string operators = operators_str() + alpha_user;
  string digits_allowed = digits_allowed_str();

  // ^ is repeat
  string operands = operands_str() + string("^");
  string digits_required = digits_required_str() + string("^");
  string digits_ge_3 = digits_ge_3_str();
  string digits = digits_str();

  operand = '\0';
  poly_size = 0;

  int op_count = 0;
  for (unsigned int i = 0; i < cn_string.length(); i++) {
    if (operators.find(cn_string[i]) != string::npos) {
      if (operand != '\0')
        return i + 1;

      if (delayed_write) {
        operations.push_back(new ops(op_count++, current_op, num_val));
        delayed_write = false;
      }

      current_op = cn_string[i];

      if (digits_allowed.find(current_op) == string::npos)
        operations.push_back(new ops(op_count++, current_op, num_val));
      else
        delayed_write = true;
    }
    else if (operands.find(cn_string[i]) != string::npos) {
      if (operand != '\0')
        return i + 1;

      if (delayed_write) {
        operations.push_back(new ops(op_count++, current_op, num_val));
        delayed_write = false;
      }

      operand = cn_string[i];
      current_op = '\0';
    }
    // if digits
    else if (digits.find(cn_string[i]) != string::npos) {
      // only operands which require a number
      if (operand != '\0') {
        if (digits_required.find(operand) == string::npos) {
          return i + 1;
        }
      }
      // if not a numeric allowed operator
      else if (digits_allowed.find(current_op) == string::npos) {
        return i + 1;
      }

      int digits_start = i;
      int digits_end = i;
      while ((digits_end + 1 < (int)cn_string.length()) &&
             (digits.find(cn_string[digits_end + 1]) != string::npos))
        digits_end++;

      number_string =
          cn_string.substr(digits_start, (digits_end - digits_start + 1));

      // if repeat character
      if (operand == '^') {
        int sz = (int)operations.size();
        if (!sz) {
          fprintf(stderr, "repeat operator is a beginning of string\n");
          return i;
        }
        int num_repeats = atoi(number_string.c_str());
        if (!num_repeats) {
          fprintf(stderr, "repeat count must be greater than 0\n");
          return i + 1;
        }

        ops *last = operations[sz - 1];
        char op = last->op;
        int op_var = last->op_var;

        for (int i = 0; i < num_repeats - 1; i++)
          operations.push_back(new ops(op_count++, op, op_var));
        operand = '\0';
      }
      // will be at end of string as it is an operand, P, A, Y, Z, R
      else if (digits_required.find(operand) != string::npos) {
        poly_size = atoi(number_string.c_str());
        if (poly_size < 3) {
          fprintf(stderr,
                  "P(n), A(n), Y(n), Z(n), R(n) n must be 3 or greater\n");
          return i + 1;
        }
      }
      // L must be 0 or 1, 3 or greater
      else if (current_op == 'L') {
        num_val = atoi(number_string.c_str());
        if (num_val == 2) {
          fprintf(stderr, "L(n), n must 0, 1, 3 or greater\n");
          return i + 1;
        }
        operations.push_back(new ops(op_count++, current_op, num_val));
        delayed_write = false;
      }
      // K, k, l must be 3 or greater, t can be 2 or greater
      else if (digits_ge_3.find(current_op) != string::npos) {
        num_val = atoi(number_string.c_str());
        if (current_op == 't') {
          if (num_val < 2) {
            fprintf(stderr, "%c(n), n must be 2 or greater\n", current_op);
            return i + 1;
          }
        }
        else if (num_val < 3) {
          fprintf(stderr, "%c(n), n must be 3 or greater\n", current_op);
          return i + 1;
        }
        operations.push_back(new ops(op_count++, current_op, num_val));
        delayed_write = false;
      }
      // operators allowing 0 or greater
      else if (digits_allowed.find(current_op) != string::npos) {
        num_val = atoi(number_string.c_str());
        if (num_val < 0) {
          fprintf(stderr, "%c(%d), n must be 0 or greater\n", current_op,
                  num_val);
          return i + 1;
        }
        operations.push_back(new ops(op_count++, current_op, num_val));
        delayed_write = false;
      }

      num_val = 1;
      i = digits_end;
    }
    // character was not found to be valid
    else {
      // fprintf(stderr,"unexpected character %c in operation string\n",
      // cn_string[i]);
      return i + 1;
    }
  }

  // if an allowed numeric operation was specified alone at end
  if (delayed_write)
    operations.push_back(new ops(op_count++, current_op, num_val));

  // if P, A or Y was specified with no digit n
  if ((digits_required.find(operand) != string::npos) && poly_size == 0) {
    fprintf(stderr, "P(n), A(n), Y(n), Z(n), R(n), n must be 3 or greater\n");
    return cn_string.length();
  }

  return 0;
}

// G. Hart Commentary
// P4 --> C    (C is prism)
// A3 --> O    (O is antiprism)
// Y3 --> T    (T is pyramid)
// e  --> aa   (abbr. for explode)
// b  --> ta   (abbr. for bevel)
// o  --> jj   (abbr. for ortho)
// m  --> kj   (abbr. for meta)
// t(n) --> dk(n)d  (dual operations) (see special case)
// j  --> dad  (dual operations)
// s  --> dgd  (dual operations)
// dd --> null (order 2)
// ad --> a    (a_ = ad_)
// gd --> g    (g_ = gd_)
// aY --> A    (interesting fact)
// dT --> T    (self-dual)
// gT --> D    (symm change)
// aT --> O    (symm change)
// dC --> O    (dual pair)
// dO --> C    (dual pair)
// dI --> D    (dual pair)
// dD --> I    (dual pair)
// aO --> aC   (for uniqueness)
// aI --> aD   (for uniqueness)
// gO --> gC   (for uniqueness)
// gI --> gD   (for uniqueness)

struct ResolveItem {
  const char *target;
  const char *resolve;
};

// clang-format off
// order sensative, do not sort
ResolveItem resolve_item_list[] = {
    {  "P4", "C",   },
    {  "A3", "O",   },
    {  "Y3", "T",   },
    {  "e",  "aa",  },
    {  "b",  "ta",  },
    {  "o",  "jj",  },
    {  "m",  "kj",  },
    {  "t",  "dk",  },
    {  "j",  "dad", },
    {  "s",  "dgd", },
    {  "dd", "",    },
    {  "ad", "a",   },
    {  "gd", "g",   },
    {  "aY", "A",   },
    {  "dT", "T",   },
    {  "gT", "D",   },
    {  "aT", "O",   },
    {  "dC", "O",   },
    {  "dO", "C",   },
    {  "dI", "D",   },
    {  "dD", "I",   },
    {  "aO", "aC",  },
    {  "aI", "aD",  },
    {  "gO", "gC",  },
    {  "gI", "gD",  },
};
// clang-format on

string resolved_cn_string(const string &cn_string)
{
  string resolve_string = cn_string;

  int num_subst = sizeof(resolve_item_list) / sizeof(resolve_item_list[0]);

  string target;
  string resolve;

  string digits = digits_str();

  // first 3 targets are positional
  if (resolve_string.length() > 1) {
    string tmp = resolve_string.substr(resolve_string.length() - 2, 2);
    for (unsigned int i = 0; i < 3; i++) {
      target = resolve_item_list[i].target;
      resolve = resolve_item_list[i].resolve;
      if (tmp == target)
        resolve_string.replace(resolve_string.length() - 2, target.length(),
                               resolve);
    }
  }

  // 4 to 6
  for (unsigned int i = 3; i < 7; i++) {
    target = resolve_item_list[i].target;
    resolve = resolve_item_list[i].resolve;
    for (size_t pos = resolve_string.find_first_of(target, 0);
         pos != string::npos; pos = resolve_string.find_first_of(target, 0)) {
      // RK - have to check now since some operators can have a number
      if ((pos + 1 == resolve_string.length()) ||
          (digits.find(resolve_string[pos + 1]) == string::npos))
        resolve_string.replace(pos, target.length(), resolve);
      else {
        // RK - if so e, b, o and m are temporarily replaced with @, #, $, %
        // so that loop can continue
        if (resolve_string[pos] == 'e')
          resolve_string.replace(pos, 1, "@");
        else if (resolve_string[pos] == 'b')
          resolve_string.replace(pos, 1, "#");
        else if (resolve_string[pos] == 'o')
          resolve_string.replace(pos, 1, "$");
        else if (resolve_string[pos] == 'm')
          resolve_string.replace(pos, 1, "%");
      }
    }
  }

  // if temporary characters exists
  replace(resolve_string.begin(), resolve_string.end(), '@', 'e');
  replace(resolve_string.begin(), resolve_string.end(), '#', 'b');
  replace(resolve_string.begin(), resolve_string.end(), '$', 'o');
  replace(resolve_string.begin(), resolve_string.end(), '%', 'm');

  // 7 is special case
  target = resolve_item_list[7].target;
  resolve = resolve_item_list[7].resolve;
  for (size_t pos = resolve_string.find_first_of(target, 0);
       pos != string::npos; pos = resolve_string.find_first_of(target, 0)) {
    resolve_string.replace(pos, target.length(), resolve);
    size_t pos2 = resolve_string.find_first_not_of(digits, pos + 2);
    if (pos2 != string::npos)
      resolve_string.insert(pos2, "d");
    else
      resolve_string.append("d");
  }

  // 8 to end. Because of length greater than 1 of the target, string.find and
  // replace cannot be used
  for (int i = 8; i < num_subst; i++) {
    target = resolve_item_list[i].target;
    resolve = resolve_item_list[i].resolve;
    int j = 0;
    int stop = resolve_string.length() - target.length() + 1;
    while (j <= stop) {
      string tmp = resolve_string.substr(j, target.length());
      if (tmp == target) {
        resolve_string.replace(j, target.length(), resolve);
        j = -1; // we have to begin looking from the begining once again, ok if
                // we modify stop
        stop = resolve_string.length() - target.length() + 1;
      }
      j++;
    }
  }

  if (resolve_string != cn_string) {
    fprintf(stderr, "%s resolved to: ", cn_string.c_str());
    if (resolve_string.length() == 0)
      fprintf(stderr, "%s", "NOTHING");
    else
      fprintf(stderr, "%s", resolve_string.c_str());
    fprintf(stderr, "\n");
  }

  return resolve_string;
}

class cn_opts : public ProgramOpts {
public:
  string ifile;
  string ofile;

  string cn_string;
  bool resolve_ops;
  bool hart_mode;
  bool tile_mode;
  bool reverse_ops;
  char operand;
  int poly_size;
  char planarize_method;
  bool planarize_method_set;
  int num_iters_planar;
  int rep_count;
  bool unitize;
  bool verbosity;
  char face_coloring_method;
  int face_opacity;
  string face_pattern;
  int seed_coloring_method;

  double epsilon;

  Color vert_col;
  Color edge_col;

  Tiling::ColoringType col_type;
  bool color_by_value;

  ColorMapMulti map;

  vector<ops *> operations;

  // for on the fly user operators
  string alpha_user;
  std::map<char, vector<ops *>> operations_user;

  cn_opts()
      : ProgramOpts("conway"), cn_string(""), resolve_ops(false),
        hart_mode(false), tile_mode(false), reverse_ops(false), operand('\0'),
        poly_size(0), planarize_method('p'), planarize_method_set(false),
        num_iters_planar(1000), rep_count(-1), unitize(false), verbosity(false),
        face_coloring_method('n'), face_opacity(-1), face_pattern("1"),
        seed_coloring_method(1), epsilon(0),
        vert_col(Color(255, 215, 0)),   // gold
        edge_col(Color(211, 211, 211)), // lightgrey
        col_type(Tiling::ColoringType::path_index), color_by_value(true)
  {
  }

  void process_command_line(int argc, char **argv);
  void usage();
};

// clang-format off
void extended_help()
{
  fprintf(stdout,
"\n"
"Conway Notation was described by Mathematician John Conway to George Hart in\n"
"the late 1990's for a book they planned to coauthor. Due to an illness the book\n"
"never came to fruition and John Conway did not think there was enough a for a\n"
"separate publication. Conway gave George Hart permission to present it in\n"
"\"Sculpture Based on Propellorized Polyhedra in the Proceedings of MOSAIC 2000\"\n"
"The paper can be viewed here: http://www.georgehart.com/propello/propello.html\n"
"The project was expected to encourage more operations to be developed which has\n"
"happened in various places including here at Antiprism. (www.antiprism.com)\n"
"\n"
"The following is a description of Conway Notation edited from the Conway\n"
"Notation web page by George W. Hart (http://www.georgehart.com)\n"
"\n"
"More detailed information and examples can be found at\n"
"http://www.georgehart.com/virtual-polyhedra/conway_notation.html\n"
"and at\n"
"http://en.wikipedia.org/wiki/Conway_polyhedron_notation\n"
"\n"
"Basics: In this notation, one specifies a \"seed\" polyhedron with a capital\n"
"letter. Operations to perform on any polyhedron are specified with lower-case\n"
"letters preceding it. This program contains a small set of seeds and operators\n"
"from which an infinite number of derived polyhedra can be generated.\n"
"\n"
"Note: This C++ port of Conway Notation can also operate on OFF files from\n"
"standard input if the seed polyhedron is not specified. (Antiprism Extension)\n"
"\n"
"Seeds: The platonic solids are denoted T, O, C, I, and D, according to their\n"
"first letter. Other polyhedra which are implemented here include prisms, Pn,\n"
"antiprisms, An, and pyramids, Yn, where n is a number (3 or greater) which you\n"
"specify to indicate the size of the base you want, e.g., Y3=T, P4=C, and A3=O.\n"
"\n"
"Operations: Currently, abdegjkmoprst are defined. They are motivated by the\n"
"operations needed to create the Archimedean solids and their duals from the\n"
"platonic solids.  Try each on a cube:\n"
"\n"
"\n(Antiprism Extenstion: note that more operations have since been defined)\n"
"\n"
"a = ambo   The ambo operation can be thought of as truncating to the edge\n"
"midpoints.  It produces a polyhedron, aX, with one vertex for each edge of X.\n"
"There is one face for each face of X and one face for each vertex of X.\n"
"Notice that for any X, the vertices of aX are all 4-fold, and that aX=adX.\n"
"If two mutually dual polyhedra are in \"dual position,\" with all edges tangent\n"
"to a common sphere, the ambo of either is their intersection.  For example\n"
"aC=aO is the cuboctahedron.\n"
"Note: ambo is also known as \"rectifying\" the polyhedron, or rectification\n"
"\n"
"b = bevel  The bevel operation can be defined by bX=taX.  bC is the truncated\n"
"cuboctahedron.  (Antiprism Extension: or \"bn\" where n is 0 or greater)\n"
"Note: bevel is also known as \"omnitruncating\" the polyhedron, or omnitruncation\n"
"\n"
"d = dual   The dual of a polyhedron has a vertex for each face, and a face for\n"
"each vertex, of the original polyhedron, e.g., dC=O.  Duality is an operation\n"
"of order two, meaning for any polyhedron X, ddX=X, e.g., ddC=dO=C.\n" 
"\n"
"e = expand This is Mrs. Stott's expansion operation.  Each face of X is\n"
"separated from all its neighbors and reconnected with a new 4-sided face,\n"
"corresponding to an edge of X.  An n-gon is then added to connect the 4-sided\n"
"faces at each n-fold vertex.  For example, eC is the rhombicuboctahedron.  It\n"
"turns out that eX=aaX and so eX=edX (Antiprism: or \"en\" where n is 0 or greater)\n"
"Note: expand is also known as \"cantellating\" the polyhedron, or cantellation\n"
"\n"
"g = gyro   The dual operation to s is g. gX=dsdX=dsX, with all 5-sided faces.\n"
"The gyrocube, gC=gO=\"pentagonal icositetrahedron,\" is dual to the snub cube.\n"
"g is like k but with the new edges connecting the face centers to the 1/3\n"
"points on the edges rather than the vertices.\n"
"\n"
"j = join   The join operator is dual to ambo, so jX=dadX=daX.  jX is like kX\n"
"without the original edges of X.  It produces a polyhedron with one 4-sided\n"
"face for each edge of X.  For example, jC=jO is the rhombic dodecahedron.\n"
"\n"
"k = kis    All faces are processed or kn = just n-sided faces are processed\n"
"The kis operation divides each n-sided face into n triangles.  A new vertex is\n"
"added in the center of each face, e.g., the kiscube, kC, has 24 triangular\n"
"faces.  The k operator is dual to t, meaning kX=dtdX.\n"
"\n"
"m = meta   Dual to b, mX=dbX=kjX.  mC has 48 triangular faces.  m is like k\n"
"and o combined; new edges connect new vertices at the face centers to the old\n"
"vertices and new vertices at the edge midpoints.  mX=mdX.  mC is the\n"
"\"hexakis octahedron.\"  (Antiprism Extension: or \"mn\" where n is 0 or greater)\n"
"\n"
"o = ortho  Dual to e, oX=deX=jjX.  oC is the trapezoidal icositetrahedron, with\n"
"24 kite-shaped faces.  oX has the effect of putting new vertices in the middle\n"
"of each face of X and connecting them, with new edges, to the edge midpoints of\n"
"X.  (Antiprism Extension: or \"on\" where n is 0 or greater)\n"
"\n"
"p = propellor    Makes each n-gon face into a \"propellor\" of an n-gon\n"
"surrounded by n quadrilaterals, e.g., pT is the tetrahedrally stellated\n"
"icosahedron. Try pkD and pt6kT. p is a self-dual operation, i.e., dpdX=pX and\n"
"dpX=pdX, and p also commutes with a and j, i.e., paX=apX. (This and the next\n"
"are extensions were added by George Hart and not specified by Conway)\n"
"\n"
"r = reflect   Changes a left-handed solid to right handed, or vice versa, but\n"
"has no effect on a reflexible solid. So rC=C, but compare sC and rsC.\n"
"\n"
"s = snub   The snub operation produces the snub cube, sC, from C.  It can be\n"
"thought of as eC followed by the operation of slicing each of the new 4-fold\n"
"faces along a diagonal into two triangles.  With a consistent handedness to\n"
"these cuts, all the vertices of sX are 5-fold.  Note that sX=sdX.\n"
"\n"
"t = truncate  All faces are processed or tn = just n-sided faces are processed\n"
"Truncating a polyhedron cuts off each vertex, producing a new n-sided face for\n"
"each n-fold vertex.  The faces of the original polyhedron still appear, but\n"
"have twice as many sides, e.g., the tC has six octagonal sides corresponding to\n"
"the six squares of the C, and eight triangles corresponding to the cube's eight\n"
"vertices.\n"
"\n"
"\n"
"Antiprism Extension: Further operations added. Also see\n"
"https://en.wikipedia.org/wiki/Conway_polyhedron_notation\n"
"\n"
"c = chamfer   New hexagonal faces are added in place of edges\n"
"\n"
"J = joined-medial  Like medial, but new rhombic faces in place of original edges\n"
"\n"
"K = stake     Subdivide faces with central quads, and triangles\n"
"              All faces processed or can be \"Kn\" where n is 3 or greater\n"
"\n"
"L0 = joined-lace  Similar to lace, except new with quad faces across original\n"
"                  edges\n"
"\n"
"L = lace      An augmentation of each face by an antiprism, adding a twist\n"
"              smaller copy of each face, and triangles between\n"
"              All faces processed or can be \"Ln\" where n is 3 or greater\n"
"\n"
"l = loft      An augmentation of each face by prism, adding a smaller copy of\n"
"              each face with trapezoids between the inner and outer ones\n"
"\n"
"M = medial    Similar to meta except no diagonal edges added, creating quad\n"
"              faces. All faces processed or can be \"Mn\" where n is 0 or greater\n"
"\n"
"n = needle    Dual of truncation, triangulate with 2 triangles across every\n"
"              edge. This bisect faces across all vertices and edges, while\n"
"              removing original edges\n"
"\n"
"q = quinto    ortho followed by truncation of vertices centered on original\n"
"              faces. This create 2 new pentagons for every original edge\n"
"\n"
"S = seed      Seed form\n"
"\n"
"u = subdivide Ambo while retaining original vertices. Similar to Loop\n"
"              subdivision surface for triangle face\n"
"\n"
"w = whirl     Gyro followed by truncation of vertices centered on original\n"
"              faces. This create 2 new hexagons for every original edge\n"
"\n"
"X = cross     Combination of kis and subdivide operation. Original edges are\n"
"              divided in half, with triangle and quad faces\n"
"\n"
"z = zip       Dual of kis or truncation of the dual. This create new edges\n"
"              perpendicular to original edges, a truncation beyond \"ambo\" with\n"
"              new edges \"zipped\" between original faces. It is also called\n"
"              bitruncation\n"
"\n"
"Orientation of the input model will have an effect on chiral operations such as\n"
"snub or whirl. The orientation mode is set to positive by default. Operations\n"
"have been added to control orientation mode. The mode will remain until changed.\n"
"+ (plus sign) = positive orientation  - (minus sign) = negative orientation\n"
"Changing orientation mode can be placed anywhere in the operation string\n"
"\n"
"Summary of operators which can take a number n\n"
"\n"
"b  - n may be 0 or greater (default: 1)\n"
"e  - n may be 0 or greater (default: 1)\n"
"K   -n may be 3 or greater representing faces sides\n"
"k  - n may be 3 or greater representing vertex connections\n"
"L  - n may be 3 or greater representing face sides, or 0\n"
"M  - n may be 0 or greater (default: 1)\n"
"m  - n may be 0 or greater (default: 1)\n"
"o  - n may be 0 or greater (default: 1)\n"
"t  - n may be 2 or greater representing face sides\n"
"\n"
"Antiprism Extension: note that any operation can be repeated N time by following\n"
"it with the ^ symbol and a number greater than 0. Examples: a^3C M0^2T\n"
"\n"
"Seeds which require a number n, 3 or greater\n"
"\n"
"P  - Prism\n"
"A  - Antiprism\n"
"Y  - Pyramid\n"
"Z  - Polygon (Antiprism Extension)\n"
"R  - Random Convex Polyhedron (Antiprism Extension)\n"
"\n"
"Note: Antiprism Extensions will work on tilings. Hart algorithms (-d) will not\n"
"e.g.: unitile2d 3 | conway p -t | antiview -v 0.1 (-t for tile mode)\n"
"\n"
"Regular tilings can be constructed from base polygons. The basic tilings are:\n"
"\n"
"            One Layer  Two Layers  Three Layers...\n"
"Square:     oZ4        o2Z4        o3Z4\n"
"Hexagonal:  tkZ6       ctkZ6       cctkZ6\n"
"Triangular: ktkZ6      kctkZ6      kcctkZ6 (kis operation on Hexagonal)\n"
"\n"
"Name                   Vertex Fig  Op     String  Dual Name              String\n"
"Square                 4,4,4,4            oZ4     Square                 do2Z4\n"
"Truncated Square       4,8,8       trunc  toZ4    Tetrakis Square        dto2Z4\n"
"Snub Square            3,3,4,3,4   snub   soZ4    Cairo Pentagonal       dso2Z4\n"
"Triangular             3,3,3,3,3,3 kis    ktkZ6   Hexagonal              ddctkZ6\n"
"Hexagonal              6,6,6              tkZ6    Triangular             dkctkZ6\n"
"Trihexagonal           3,6,3,6     ambo   atkZ6   Rhombille              dactkZ6\n"
"Snub Trihexagonal      3,3,3,3,6   snub   stkZ6   Floret Pentagonal      dsctkZ6\n"
"Truncated Hexagonal    3,12,12     trunc  ttkZ6   Triakis triangular     dtctkZ6\n"
"Rhombitrihexagonal     3,4,6,4     expand etkZ6   Deltoidal Trihexagonal dectkZ6\n"
"Truncated Trihexagonal 4,6,12      bevel  btkZ6   Kisrhombille           dbctkZ6\n"
"Elongated Triangular   3,3,3,4,4   Non Wythoffian Prismatic Triangular   none\n"
"\n"
"\n"
"Substitutions used by George Hart algorithms\n"
"\n");

  int num_subst = sizeof(resolve_item_list) / sizeof(resolve_item_list[0]);
  for (int i = 0; i < num_subst; i++)
    fprintf(stdout,"%-2s -> %s\n", resolve_item_list[i].target, resolve_item_list[i].resolve);

  fprintf(stdout,
"\n"
/*
"Various equivalent forms\n"
"\n"
"jT    = C\n"
"sT    = I\n"
"dA3   = C\n"
"k5A5  = I (A special gyroelongated dipyramid)\n"
"t5dA5 = D (A special truncated trapezohedron)\n"
"t4daC = cC\n"
"t4kC  = lC\n"
"daC   = jC\n"
"t5daaD or t5deD or t5oD = qD\n"
"dedD   = oD\n"
*/
"Equivalent Operations\n"
"\n"
"b0 = z        e0 = d        o0 = S        m0 = k        M0 = o\n"
"b1 = b        e1 = e        o1 = o        m1 = m        M1 = M\n"
"\n");
}

void cn_opts::usage()
{
   fprintf(stdout,
"\n"
"Usage: %s [options] [Conway Notation string] [input_file]\n"
"\n"
"Conway Notation uses algorithms by George W. Hart (http://www.georgehart.com)\n"
"http://www.georgehart.com/virtual-polyhedra/conway_notation.html\n"
"\n"
"Antiprism Extensions: Further operations added. See\n"
"https://en.wikipedia.org/wiki/Conway_polyhedron_notation\n"
"\n"
"Read a polyhedron from a file in OFF format.\n"
"If input_file is not given and no seed polyhedron is given in the notation\n"
"string then the program reads from standard input.\n"
"\n"
"Options\n"
"%s"
"  -H        Conway Notation detailed help. seeds and operator descriptions\n"
"  -s        apply Conway Notation string substitutions\n"
"  -g        use George Hart algorithms (sets -s, -p m or -p x cannot be used)\n"
"  -c <op=s> user defined operation strings in the form of op,string\n"
"              op can be any operation letter not currently in use\n"
"              string can be any operations. More than one <op=s> can be used\n"
"              Examples: -c x=kt,y=tk,v=dwd or -c x=kt -c y=tk -c v=dwd\n"
"  -t        tile mode. when input is a tiling. unsets -g  sets -p u\n"
"              set if seed of Z is detected\n"
"  -r        execute operations in reverse order (left to right)\n"
"  -u        make final product be averge unit edge length\n"
"  -v        verbose output\n"
"  -o <file> write output to file (default: write to standard output)\n"
"\n"
"Planarization options (use canonical program to canonicalize output)\n"
"  -p <mthd> inter-step planarization method\n"
"               p - face centroids (magnitude squared) (default)\n"
"               m - mathematica planarize\n"
"               c - mathematica canonicalize\n"
"               u - make faces into unit-edged regular polygons (minmax -a u)\n"
"               x - none\n"
"  -i <itrs> maximum inter-step planarization iterations (default: 1000)\n"
"  -z <n>    status reporting every n iterations, -1 for no status (default: -1)\n"
"  -l <lim>  minimum distance change to terminate planarization, as negative\n"
"               exponent (default: %d giving %.0e)\n"
"\n"
"Coloring Options (run 'off_util -H color' for help on color formats)\n"
"  -V <col>  vertex color (default: gold)\n"
"  -E <col>  edge color   (default: lightgray)\n"
"  -f <mthd> mthd is face coloring method using color in map (default: n)\n"
"               key word: none - sets no color\n"
"               n - by number of sides\n"
"               s - symmetric coloring\n"
"               o - newly created faces by operation\n"
"               w - resolve color indexes (overrides -V and -E)\n"
"  -C <mthd> colouring method for tiles: none, index, value, association\n"
"            (default: index) index and value methods use the path index,\n"
"            association associates tiles with base geometry element colours\n"
"               (when -f w is set)\n"
"  -R <opt>  built in seed coloring: one=1, unique=2, symmetry=3 (default: 1)\n"
"  -T <tran> face transparency. valid range from 0 (invisible) to 255 (opaque)\n"
"  -O <strg> face transparency pattern string (-f n only). valid values\n"
"               0 - map color alpha value, 1 -T alpha applied (default: '1')\n"
"  -m <maps> color maps for faces to be tried in turn (default: m1, for -g, m2)\n"
"               keyword m1: red,darkorange1,yellow,darkgreen,cyan,blue,magenta,\n"
"                           white,grey,black\n"
"               keyword m2: red,blue,green,yellow,brown,magenta,purple,grue,\n"
"                           gray,orange (from George Hart\'s original applet)\n"
"\n"
"\n",prog_name(), help_ver_text, int(-log(::epsilon)/log(10) + 0.5), ::epsilon);
}
// clang-format on

void cn_opts::process_command_line(int argc, char **argv)
{
  opterr = 0;
  int c;
  int op_term = 0;

  string alphas_in_use = operators_str() + operands_str();

  int sig_compare = INT_MAX;

  string map_file;

  handle_long_opts(argc, argv);

  while ((c = getopt(argc, argv, ":hHsgtruvc:p:l:i:z:f:C:R:V:E:T:O:m:o:")) !=
         -1) {
    if (common_opts(c, optopt))
      continue;

    switch (c) {
    case 'H':
      extended_help();
      exit(0);

    case 's':
      resolve_ops = true;
      break;

    case 'g':
      hart_mode = true;
      resolve_ops = true;
      break;

    case 't':
      tile_mode = true;
      break;

    case 'r':
      reverse_ops = true;
      break;

    case 'u':
      unitize = true;
      break;

    case 'v':
      verbosity = true;
      break;

    case 'c': {
      char operation = '\0';
      string user_op;

      char parse_key1[] = ",";

      // memory pointer for strtok_r
      char *tok_ptr1;

      vector<string> tokens;
      char *ptok1 = strtok_r(optarg, parse_key1, &tok_ptr1);
      while (ptok1 != nullptr) {
        tokens.push_back(ptok1);
        ptok1 = strtok_r(nullptr, parse_key1, &tok_ptr1);
      }

      for (int i = 0; i < (int)tokens.size(); i++) {
        op_term++;

        char parse_key2[] = "=";

        // memory pointer for strtok_r
        char *tok_ptr2;

        // string to char * (not const) from StackOverflow
        auto *writable = new char[tokens[i].size() + 1];
        copy(tokens[i].begin(), tokens[i].end(), writable);
        writable[tokens[i].size()] = '\0';

        char *ptok2 = strtok_r(writable, parse_key2, &tok_ptr2);
        int count1 = 0;
        while (ptok2 != nullptr) {
          if (count1 == 0) {
            if (strlen(ptok2) != 1)
              error(msg_str("term %d: operation must be one character '%s'",
                            op_term, ptok2),
                    c);
            else if (!isalpha(ptok2[0])) {
              error(msg_str("term %d: operation must be alphabetic '%s'",
                            op_term, ptok2),
                    c);
            }
            else if (alphas_in_use.find(ptok2[0]) != string::npos) {
              error(msg_str("term %d: operation character already in use '%s'",
                            op_term, ptok2),
                    c);
            }
            else {
              operation = ptok2[0];
              alphas_in_use += operation;
              alpha_user += operation;
            }
          }
          else if (count1 == 1) {
            user_op = ptok2;
          }
          else {
            error(msg_str("term %d: unexpected parameter '%s'", op_term, ptok2),
                  c);
          }

          ptok2 = strtok_r(nullptr, parse_key2, &tok_ptr2);
          count1++;
        }

        char operand_test;
        int dummy1;
        string dummy2;
        if (int pos = validate_cn_string(user_op, operations_user[operation],
                                         operand_test, dummy1, dummy2))
          error(msg_str("term %d: unexpected character in position %d: %c",
                        op_term, pos, user_op[pos - 1]),
                c);

        if (operand_test)
          error(msg_str("term %d: cannot contain operand in position %d: %c",
                        op_term, user_op.length(), operand_test),
                c);

        delete[] writable;
      }

      break;
    }

    case 'p':
      planarize_method_set = true;
      if (strlen(optarg) == 1 && strchr("pmcux", int(*optarg)))
        planarize_method = *optarg;
      else
        error("planarize method type must be p, m, c, u or x", c);
      break;

    case 'l':
      print_status_or_exit(read_int(optarg, &sig_compare), c);
      if (sig_compare < 0) {
        warning("limit is negative, and so ignored", c);
      }
      if (sig_compare > DEF_SIG_DGTS) {
        warning("limit is very small, may not be attainable", c);
      }
      break;

    case 'i':
      print_status_or_exit(read_int(optarg, &num_iters_planar), c);
      if (num_iters_planar < 0)
        error("number of planarization iterations 0 or greater", c);
      break;

    case 'z':
      print_status_or_exit(read_int(optarg, &rep_count), c);
      if (rep_count < -1)
        error("number of planar report iterations must be -1 or greater", c);
      break;

    case 'f':
      if (!strcasecmp(optarg, "none"))
        face_coloring_method = '\0';
      else if (strspn(optarg, "nosw") != strlen(optarg) || strlen(optarg) > 1)
        error(msg_str("invalid face Coloring method '%s'", optarg), c);
      else {
        face_coloring_method = *optarg;
      }
      break;

    case 'C': {
      string arg_id;
      print_status_or_exit(
          get_arg_id(optarg, &arg_id, "none=0|index=1|value=2|association=3"),
          c);
      int id = atoi(arg_id.c_str());
      typedef Tiling::ColoringType CT;
      if (id == 0)
        col_type = CT::none;
      else if (id == 1 || id == 2)
        col_type = CT::path_index;
      else
        col_type = CT::associated_element;

      color_by_value = (id != 1);
      break;
    }

    case 'R': {
      string arg_id;
      print_status_or_exit(get_arg_id(optarg, &arg_id,
                                      "one=1|unique=2|symmetry=3",
                                      argmatch_add_id_maps),
                           c);
      seed_coloring_method = atoi(arg_id.c_str());
      break;
    }

    case 'V':
      print_status_or_exit(vert_col.read(optarg), c);
      break;

    case 'E':
      print_status_or_exit(edge_col.read(optarg), c);
      break;

    case 'T':
      print_status_or_exit(read_int(optarg, &face_opacity), c);
      if (face_opacity < 0 || face_opacity > 255) {
        error("face transparency must be between 0 and 255", c);
      }
      break;

    case 'O':
      if (strspn(optarg, "01") != strlen(optarg))
        error(msg_str("transparency string is '%s' must consist of "
                      "0 and 1's",
                      optarg),
              c);
      face_pattern = optarg;
      break;

    case 'm':
      map_file = optarg;
      break;

    case 'o':
      ofile = optarg;
      break;

    default:
      error("unknown command line error");
    }
  }

  if (argc - optind > 2)
    error("too many arguments");

  if (argc - optind > 0)
    cn_string = argv[optind];
  else
    error("no Conway Notation string given");

  if (int pos = validate_cn_string(cn_string, operations, operand, poly_size,
                                   alpha_user))
    error(msg_str("Unexpected character in position %d: %c", pos,
                  cn_string[pos - 1]));

  if (resolve_ops) {
    cn_string = resolved_cn_string(cn_string);

    // operations is a vector of objects
    for (auto &operation : operations)
      delete operation;
    operations.clear();

    // revalidate (should be valid) to rebuild operations table
    if (int pos = validate_cn_string(cn_string, operations, operand, poly_size,
                                     alpha_user))
      error(msg_str("Unexpected character in position %d: %c", pos,
                    cn_string[pos - 1]));
  }

  if (argc - optind == 2) {
    ifile = argv[++optind];
    if (operand)
      error(
          msg_str("operand '%c' was specified so input file '%s' is unexpected",
                  operand, ifile.c_str()));
  }

  // operations can be done in reverse order
  if (!reverse_ops) {
    sort(operations.begin(), operations.end(), cmp_ops);
    for (unsigned int i = 0; i < alpha_user.length(); i++)
      sort(operations_user[alpha_user[i]].begin(),
           operations_user[alpha_user[i]].end(), cmp_ops);
  }

  // force tile mode if using polygon
  if (operand == 'Z')
    tile_mode = true;

  if (tile_mode) {
    warning("in tile mode", 't');
    if (hart_mode) {
      warning("polygons will not process correctly with George Hart "
              "algorithms. turned off",
              'g');
      hart_mode = false;
    }
    if (!planarize_method_set) {
      planarize_method = 'u';
      warning("default planarization method for tiling set to 'u'");
    }
  }

  if (hart_mode) {
    if ((planarize_method == 'm') || (planarize_method == 'x'))
      error("when -g set, planrize method must be p, c, or u", 'p');
    if ((face_coloring_method == 'o') || (face_coloring_method == 'w'))
      error("when -g set, face coloring methods o and w are invalid", 'f');
  }

  // when use George Hart algorithms, use map he used on line
  if (!map_file.size())
    map_file = (hart_mode) ? "m2" : "m1";

  if (map_file == "m1" || map_file == "m2") {
    auto *col_map = new ColorMapMap;
    if (map_file == "m1") {
      col_map->set_col(0, Color(1.0, 0.0, 0.0)); // 3-sided faces red
      col_map->set_col(1,
                       Color(1.0, 0.49804, 0.0)); // 4-sided faces darkoranage1
      col_map->set_col(2, Color(1.0, 1.0, 0.0));  // 5-sided faces yellow
      col_map->set_col(3, Color(0.0, 0.39216, 0.0)); // 6-sided faces darkgreen
      col_map->set_col(4, Color(0.0, 1.0, 1.0));     // 7-sided faces cyan
      col_map->set_col(5, Color(0.0, 0.0, 1.0));     // 8-sided faces blue
      col_map->set_col(6, Color(1.0, 0.0, 1.0));     // 9-sided faces magenta
      col_map->set_col(7, Color(1.0, 1.0, 1.0));     // 10-sided faces white
      col_map->set_col(8, Color(0.5, 0.5, 0.5));     // 11-sided faces grey
      col_map->set_col(9, Color(0.0, 0.0, 0.0));     // 12-sided faces black
    }
    else if (map_file == "m2") {
      auto *col_map0 = new ColorMapMap;
      col_map0->set_col(0, Color(0.9, 0.3, 0.3));   // 3-sided faces red
      col_map0->set_col(1, Color(0.4, 0.4, 1.0));   // 4-sided faces blue
      col_map0->set_col(2, Color(0.2, 0.9, 0.3));   // 5-sided faces green
      col_map0->set_col(3, Color(0.9, 0.9, 0.2));   // 6-sided faces yellow
      col_map0->set_col(4, Color(0.5, 0.25, 0.25)); // 7-sided faces brown
      col_map0->set_col(5, Color(0.8, 0.2, 0.8));   // 8-sided faces magenta
      col_map0->set_col(6, Color(0.5, 0.2, 0.8));   // 9-sided faces purple
      col_map0->set_col(7, Color(0.1, 0.9, 0.9));   // 10-sided faces grue
      col_map0->set_col(8, Color(0.5, 0.5, 0.5));   // 11-sided faces gray
      col_map0->set_col(9, Color(1.0, 0.6, 0.1));   // 12-sided faces orange
      map.add_cmap(col_map0);

      // George Hart had all higher faces at grey
      col_map->set_col(0, Color(0.5, 0.5, 0.5)); // 13-sided faces and higher
    }
    col_map->set_wrap();
    map.add_cmap(col_map);
  }
  else
    print_status_or_exit(map.init(map_file.c_str()), 'm');

  epsilon = (sig_compare != INT_MAX) ? pow(10, -sig_compare) : ::epsilon;
}

void verbose(char operation, int op_var, const cn_opts &opts)
{
  if (opts.verbosity) {
    string operator_name;

    int last_op =
        sizeof(conway_operator_list) / sizeof(conway_operator_list[0]);
    for (int i = 0; i < last_op; i++) {
      if (operation == conway_operator_list[i].operator_short[0]) {
        operator_name = conway_operator_list[i].operator_name;
        break;
      }
    }

    string hart_operators = "agkp";
    string hart_string;
    if (opts.hart_mode && (hart_operators.find(operation) != string::npos))
      hart_string = "(hart)";

    // special cases
    char buf[MSG_SZ];
    buf[0] = '\0';
    bool user_op = false;
    if (opts.alpha_user.find(operation) != string::npos) {
      user_op = true;
      operator_name = "user operation: ";
    }
    else if (operation == '_')
      operator_name = "planarizing ...";
    else if (operation == '+')
      operator_name = "orient positive mode";
    else if (operation == '-')
      operator_name = "orient negative mode";
    else if (operation == '@')
      operator_name = "non-orientable geometry";
    else if (operation == '$')
      operator_name = "done.";
    // L and op_var is -1 means L stands alone
    else if (operation == 'L' && op_var == -1) {
      buf[0] = '\0';
    }
    // if L may have a 0
    else if (operation == 'L' && op_var == 0) {
      operator_name = "joined-lace";
    }
    // all other case show op_var when not 1
    else if (op_var != 1)
      sprintf(buf, "(%d)", op_var);

    fprintf(stderr, "%s%c%s %s\n", operator_name.c_str(),
            (user_op ? operation : '\0'), buf, hart_string.c_str());
  }
}

void unitize_edges(Geometry &geom)
{
  GeometryInfo info(geom);
  if (info.num_iedges() > 0) {
    double val = info.iedge_length_lims().sum / info.num_iedges();
    geom.transform(Trans3d::scale(1 / val));
  }
}

void centroid_to_origin(Geometry &geom)
{
  geom.transform(Trans3d::translate(-centroid(geom.verts())));
}

/*
// RK - average radius rather than maximum has more reliability than max
void unitize_vertex_radius(Geometry &geom)
{
  GeometryInfo info(geom);
  info.set_center(geom.centroid());
  // geom.transform(Trans3d::scale(1 / info.vert_dist_lims().max));
  double avg = info.vert_dist_lims().sum / info.num_verts();
  geom.transform(Trans3d::scale(1 / avg));
}
*/

void cn_planarize(Geometry &geom, char planarize_method, const cn_opts &opts)
{
  if ((opts.num_iters_planar != 0) && (opts.planarize_method != 'x')) {
    verbose('_', 0, opts);
    if (planarize_method == 'p')
      planarize_bd(geom, opts.num_iters_planar, opts.rep_count, opts.epsilon);
    else if (planarize_method == 'm')
      planarize_mm(geom, opts.num_iters_planar, opts.rep_count, opts.epsilon);
    else if (planarize_method == 'c') {
      // RK - need?
      // unitize_vertex_radius(geom);
      // geom.transform(Trans3d::translate(-centroid(geom.verts())));
      canonicalize_mm(geom, opts.num_iters_planar, opts.rep_count,
                      opts.epsilon);
    }
    else if (planarize_method == 'u') {
      minmax_unit_planar(geom, opts.num_iters_planar, opts.rep_count,
                         opts.epsilon);
    }
    // note: sometimes radius becomes very small with option p
    // if unitizing faces, don't alter radius
    if (planarize_method != 'u')
      unitize_nearpoints_radius(geom);
  }
}

void get_operand(Geometry &geom, const cn_opts &opts)
{
  string uniforms = "TCOID";

  if (uniforms.find(opts.operand) != string::npos) {
    switch (opts.operand) {
    case 'T':
      geom.read_resource("std_tet");
      break;

    case 'C':
      geom.read_resource("std_cube");
      break;

    case 'O':
      geom.read_resource("std_oct");
      break;

    case 'I':
      geom.read_resource("std_ico");
      break;

    case 'D':
      geom.read_resource("std_dod");
      break;
    }
  }
  else {
    Polygon pgon(opts.poly_size, 1);

    switch (opts.operand) {
    case 'P':
      pgon.set_type(Polygon::prism);
      break;

    case 'A':
      pgon.set_type(Polygon::antiprism);
      break;

    case 'Y':
      pgon.set_type(Polygon::pyramid);
      break;

    case 'Z':
      pgon.set_type(Polygon::dihedron);
      pgon.set_subtype(Polygon::sub_dihedron_polygon);
      break;

    // a polyhedron of random points
    case 'R':
      Random rnd;
      rnd.time_seed();
      for (int i = 0; i < opts.poly_size; i++)
        geom.add_vert(Vec3d::random(rnd).unit());
      geom.set_hull();
      break;
    }

    pgon.set_edge(0, 1.0);

    if (opts.operand == 'Y' && opts.poly_size > 5)
      // Based on circumradius
      pgon.set_height(0, (1 / sin(M_PI / opts.poly_size)) / 2);
    // inradius
    // poly->set_height((1/tan(M_PI/poly_size))/2);
    else
      pgon.set_edge(1, 1.0);

    pgon.make_poly(geom);

    /* RK - if polygon size 2 was allowed, caused too much trouble
    if ((opts.poly_size == 2) && (opts.operand == 'P' || opts.operand == 'Y'))
      geom.transform(Trans3d::rotate(deg2rad(90), 0, 0));
    */
  }

  // by default seed will be all one color
  Coloring clrng(&geom);
  if (opts.seed_coloring_method == 1) {
    Color col = opts.map.get_col(0);
    clrng.vef_one_col(col, col, col);
  }
  else if (opts.seed_coloring_method == 2) {
    // unique
    clrng.v_unique(true);
    clrng.e_unique(true);
    clrng.f_unique(true);
  }
  else if (opts.seed_coloring_method == 3) {
    // color by symmetry
    Symmetry sym;
    vector<vector<set<int>>> sym_equivs;
    sym.init(geom, &sym_equivs);
    clrng.v_sets(sym_equivs[2], true);
    clrng.e_sets(sym_equivs[2], true);
    clrng.f_sets(sym_equivs[2], true);
  }
}

// RK - for hart code
void build_new_faces(map<string, map<string, string>> &faces_table,
                     map<string, int> verts_table,
                     vector<vector<int>> &faces_new)
{
  map<string, map<string, string>>::iterator ft;
  map<string, string>::iterator ftm;
  string face_name;
  for (ft = faces_table.begin(); ft != faces_table.end(); ft++) {
    for (ftm = ft->second.begin(); ftm != ft->second.end(); ftm++) {
      if (face_name != ft->first) {
        face_name = ft->first;
        string v0 = faces_table[face_name][ftm->first];
        string v = v0;
        vector<int> face;
        do {
          face.push_back(verts_table[v]);
          v = faces_table[face_name][v];
        } while (v != v0);
        if (face.size() > 2) // make sure face is valid
          faces_new.push_back(face);
        face.clear();
      }
    }
  }
}

// hart_ code ported from George Hart java
void hart_ambo(Geometry &geom)
{
  vector<vector<int>> &faces = geom.raw_faces();
  vector<Vec3d> &verts = geom.raw_verts();

  map<string, int> verts_table;
  map<string, map<string, string>> faces_table;
  vector<Vec3d> verts_new;

  char buf1[MSG_SZ], buf2[MSG_SZ], buf3[MSG_SZ];
  unsigned int vert_num = 0;
  for (unsigned int i = 0; i < faces.size(); i++) {
    int v1 = faces[i].at(faces[i].size() - 2);
    int v2 = faces[i].at(faces[i].size() - 1);
    for (unsigned int j = 0; j < faces[i].size(); j++) {
      int v3 = faces[i].at(j);
      if (v1 < v2) {
        sprintf(buf1, "%d_%d", (v1 < v2) ? v1 : v2, (v1 > v2) ? v1 : v2);
        verts_table[buf1] = vert_num++;
        verts_new.push_back((verts[v1] + verts[v2]) * 0.5);
      }
      sprintf(buf1, "f%d", i);
      sprintf(buf2, "%d_%d", (v1 < v2) ? v1 : v2, (v1 > v2) ? v1 : v2);
      sprintf(buf3, "%d_%d", (v2 < v3) ? v2 : v3, (v2 > v3) ? v2 : v3);
      faces_table[buf1][buf2] = buf3;
      sprintf(buf1, "v%d", v2);
      sprintf(buf2, "%d_%d", (v2 < v3) ? v2 : v3, (v2 > v3) ? v2 : v3);
      sprintf(buf3, "%d_%d", (v1 < v2) ? v1 : v2, (v1 > v2) ? v1 : v2);
      faces_table[buf1][buf2] = buf3;
      v1 = v2;
      v2 = v3;
    }
  }

  geom.clear_all();
  verts = verts_new;
  verts_new.clear();

  build_new_faces(faces_table, verts_table, faces);
  faces_table.clear();
  verts_table.clear();
}

void hart_gyro(Geometry &geom)
{
  vector<vector<int>> &faces = geom.raw_faces();
  vector<Vec3d> &verts = geom.raw_verts();

  map<string, int> verts_table;
  map<string, map<string, string>> faces_table;
  vector<Vec3d> verts_new;

  char buf1[MSG_SZ], buf2[MSG_SZ], buf3[MSG_SZ];
  unsigned int vert_num = 0;
  vector<Vec3d> centers;
  geom.face_cents(centers);
  for (unsigned int i = 0; i < faces.size(); i++) {
    sprintf(buf1, "f%d", i);
    verts_table[buf1] = vert_num++;
    verts_new.push_back(centers[i].unit());
  }
  centers.clear();

  for (unsigned int i = 0; i < verts.size(); i++) {
    sprintf(buf1, "v%d", i);
    verts_table[buf1] = vert_num++;
    verts_new.push_back(verts[i]);
  }

  for (unsigned int i = 0; i < faces.size(); i++) {
    int v1 = faces[i].at(faces[i].size() - 2);
    int v2 = faces[i].at(faces[i].size() - 1);
    for (unsigned int j = 0; j < faces[i].size(); j++) {
      int v3 = faces[i].at(j);
      sprintf(buf1, "%d~%d", v1, v2);
      verts_table[buf1] = vert_num++;
      // approx. (2/3)v1 + (1/3)v2
      verts_new.push_back(verts[v1] * 0.7 + verts[v2] * 0.3);

      sprintf(buf1, "%df%d", i, v1);
      sprintf(buf2, "f%d", i);
      sprintf(buf3, "%d~%d", v1, v2);
      faces_table[buf1][buf2] = buf3;
      sprintf(buf2, "%d~%d", v1, v2);
      sprintf(buf3, "%d~%d", v2, v1);
      faces_table[buf1][buf2] = buf3;
      sprintf(buf2, "%d~%d", v2, v1);
      sprintf(buf3, "v%d", v2);
      faces_table[buf1][buf2] = buf3;
      sprintf(buf2, "v%d", v2);
      sprintf(buf3, "%d~%d", v2, v3);
      faces_table[buf1][buf2] = buf3;
      sprintf(buf2, "%d~%d", v2, v3);
      sprintf(buf3, "f%d", i);
      faces_table[buf1][buf2] = buf3;

      v1 = v2;
      v2 = v3;
    }
  }

  geom.clear_all();
  verts = verts_new;
  verts_new.clear();

  build_new_faces(faces_table, verts_table, faces);
  faces_table.clear();
  verts_table.clear();
}

void hart_kisN(Geometry &geom, int n)
{
  // default num_val was changed to 1
  if (n < 3)
    n = 0;

  vector<vector<int>> &faces = geom.raw_faces();
  vector<Vec3d> &verts = geom.raw_verts();

  vector<Vec3d> centers;
  geom.face_cents(centers);

  vector<vector<int>> faces_new;
  vector<int> face;
  for (unsigned int i = 0; i < faces.size(); i++) {
    if (n > 0 && (int)faces[i].size() != n) {
      faces_new.push_back(faces[i]);
      continue;
    }

    verts.push_back(centers[i]);
    for (unsigned int j = 0; j < faces[i].size() - 1; j++) {
      face.push_back(verts.size() - 1);
      face.push_back(faces[i][j]);
      face.push_back(faces[i][j + 1]);
      faces_new.push_back(face);
      face.clear();
    }

    face.push_back(verts.size() - 1);
    face.push_back(faces[i][faces[i].size() - 1]);
    face.push_back(faces[i][0]);
    faces_new.push_back(face);
    face.clear();
  }
  centers.clear();

  if (faces_new.size() > 0) {
    faces.clear();
    faces = faces_new;
    faces_new.clear();
  }
}

void hart_propellor(Geometry &geom)
{
  vector<vector<int>> &faces = geom.raw_faces();
  vector<Vec3d> &verts = geom.raw_verts();

  map<string, int> verts_table;
  map<string, map<string, string>> faces_table;
  vector<Vec3d> verts_new;

  char buf1[MSG_SZ], buf2[MSG_SZ], buf3[MSG_SZ];
  unsigned int vert_num = 0;
  for (unsigned int i = 0; i < verts.size(); i++) {
    sprintf(buf1, "v%d", i);
    verts_table[buf1] = vert_num++;
    verts_new.push_back(verts[i].unit());
  }

  for (unsigned int i = 0; i < faces.size(); i++) {
    int v1 = faces[i].at(faces[i].size() - 2);
    int v2 = faces[i].at(faces[i].size() - 1);
    for (unsigned int j = 0; j < faces[i].size(); j++) {
      int v3 = faces[i].at(j);
      sprintf(buf1, "%d~%d", v1, v2);
      verts_table[buf1] = vert_num++;
      // approx. (2/3)v1 + (1/3)v2
      verts_new.push_back(verts[v1] * 0.7 + verts[v2] * 0.3);

      sprintf(buf1, "v%d", i);
      sprintf(buf2, "%d~%d", v1, v2);
      sprintf(buf3, "%d~%d", v2, v3);
      faces_table[buf1][buf2] = buf3;
      sprintf(buf1, "%df%d", i, v2);
      sprintf(buf2, "%d~%d", v1, v2);
      sprintf(buf3, "%d~%d", v2, v1);
      faces_table[buf1][buf2] = buf3;
      sprintf(buf2, "%d~%d", v2, v1);
      sprintf(buf3, "v%d", v2);
      faces_table[buf1][buf2] = buf3;
      sprintf(buf2, "v%d", v2);
      sprintf(buf3, "%d~%d", v2, v3);
      faces_table[buf1][buf2] = buf3;
      sprintf(buf2, "%d~%d", v2, v3);
      sprintf(buf3, "%d~%d", v1, v2);
      faces_table[buf1][buf2] = buf3;

      v1 = v2;
      v2 = v3;
    }
  }

  geom.clear_all();
  verts = verts_new;
  verts_new.clear();

  build_new_faces(faces_table, verts_table, faces);
  faces_table.clear();
  verts_table.clear();
}

/*
// chamfer for hart code
void hart_chamfer(Geometry &geom, const cn_opts &opts)
{
  vector<Vec3d> &verts = geom.raw_verts();
  int sz = verts.size();

  // make all edges a face
  // this is a join operation but retains the original vertex indexes
  make_edges_to_faces(geom);

  project_onto_sphere(geom);

  // truncate only on the new vertices
  vector<int> v_idxs;
  for (unsigned int i = sz; i < verts.size(); i++)
    v_idxs.push_back(i);

  verbose('t', 0, opts);
  truncate_verts(geom, v_idxs, CN_ONE_HALF);
}

// whirl for hart code
void hart_whirl(Geometry &geom, bool orientation_positive, const cn_opts &opts)
{
  int num_faces = geom.raw_faces().size();

  verbose('g', 0, opts);
  hart_gyro(geom);

  // after intra-step operation
  GeometryInfo info(geom);
  if (!info.is_orientable())
    verbose('@', 0, opts);
  else
    // orientation is reversed if reflected 1=positive 2=negative
    geom.orient((orientation_positive) ? 1 : 2);

  cn_planarize(geom, opts);

  // only truncate on original face centers
  vector<int> v_idxs;
  for (int i = 0; i < num_faces; i++)
    v_idxs.push_back(i);

  verbose('t', 0, opts);
  truncate_verts(geom, v_idxs, CN_ONE_HALF, nullptr);
}
*/

// operations which can use Antiprism built in features
/*
void antiprism_dual(Geometry &geom)
{
  Geometry dual;
  centroid_to_origin(geom);
  get_dual(dual, geom, 1, Vec3d(0, 0, 0));
  geom = dual;
}
*/

void antiprism_reflect(Geometry &geom, const cn_opts &opts)
{
  if (opts.tile_mode)
    geom.transform(Trans3d::reflection(Vec3d(1, 0, 0)));
  else
    geom.transform(Trans3d::inversion());
}

// built in truncate from off_util
void antiprism_truncate(Geometry &geom, double ratio, int n)
{
  truncate_verts(geom, ratio, n);
}

void orient_planar(Geometry &geom, bool &is_orientable,
                   bool &orientation_positive, const cn_opts &opts)
{
  // local copy
  char planarize_method = opts.planarize_method;

  GeometryInfo info(geom);
  is_orientable = info.is_orientable();
  if (!is_orientable) {
    verbose('@', 0, opts);
    if (!opts.planarize_method_set) {
      planarize_method = 'u';
      opts.warning("default planarization method for non-orientable geometry "
                   "set to 'u'");
    }
  }
  else
    // orientation is reversed if reflected 1=positive 2=negative
    geom.orient((orientation_positive) ? 1 : 2);

  // planarize after each step
  cn_planarize(geom, planarize_method, opts);
}

// is_orientable and orientation_positive can change
void wythoff(Geometry &geom, char operation, int op_var, int &operation_number,
             bool &is_orientable, bool &orientation_positive,
             const cn_opts &opts)
{
  operation_number++;

  string digits_ge_3 = digits_ge_3_str(); // t processed with utility
  string non_color_ops = "r+-";

  // if coloring new faces, track color of current faces
  // skip for reflections, orientation (when colors are not altered)
  vector<pair<Vec3d, Color>> color_centroids;
  if ((opts.face_coloring_method == 'o') &&
      (non_color_ops.find(operation) == string::npos)) {
    for (int i = 0; i < (int)geom.faces().size(); i++) {
      pair<Vec3d, Color> color_cent;
      color_cent.first = geom.face_cent(i);
      color_cent.second = geom.colors(FACES).get(i);
      color_centroids.push_back(color_cent);
    }
  }

  // truncate with N>1 uses Hart algorithm
  if (operation == 't' && op_var > 1)
    antiprism_truncate(geom, CN_ONE_THIRD, op_var);
  else if (operation == 'r') {
    antiprism_reflect(geom, opts);
    // decrimenting operation number gives consistent colors
    operation_number--;
  }
  else if (operation == '+') {
    orientation_positive = true;
    operation_number--;
  }
  else if (operation == '-') {
    orientation_positive = false;
    operation_number--;
  }
  else {
    Geometry geom_save;
    vector<int> dels;
    // can wythoff handle n
    bool wythoff_n = true;
    if ((digits_ge_3.find(operation) != string::npos) && (op_var > 1)) {
      wythoff_n = false;
      geom_save = geom;
      // remove all faces of size op_var from geom_save
      for (int i = 0; i < (int)geom.faces().size(); i++) {
        if ((int)geom_save.faces(i).size() == op_var)
          dels.push_back(i);
      }
      // if matching faces found
      if (dels.size()) {
        geom_save.del(FACES, dels);
        // place only those faces in geom
        geom = faces_to_geom(geom, dels);
      }
      // no faces to act on, loop
      else {
        return;
      }
    }

    // wythoff call requires a string
    string wythoff_op;
    wythoff_op.push_back(operation);

    char buf[MSG_SZ];
    if ((op_var != 1) && wythoff_n) {
      sprintf(buf, "%d", op_var);
      wythoff_op += string(buf);
    }

    // fprintf(stderr, "wythoff_op = %s\n", wythoff_op.c_str());
    opts.print_status_or_exit(wythoff_make_tiling(
        geom, geom, wythoff_op, is_orientable, false, opts.col_type));

    // remove digons
    dels.clear();
    for (int i = 0; i < (int)geom.faces().size(); i++) {
      if (geom.faces(i).size() < 3) {
        // if coloring model like wythoff, move digons to edges
        if (opts.face_coloring_method == 'w') {
          Color col = geom.colors(FACES).get(i);
          geom.add_edge(geom.faces(i), col);
        }
        dels.push_back(i);
      }
    }
    geom.del(FACES, dels);
    // remove any free vertices that were formed
    geom.del(VERTS, geom.get_info().get_free_verts());

    // check for 3 faces at an edge
    auto efpairs = geom.get_edge_face_pairs(false);
    map<vector<int>, vector<int>>::const_iterator ei;
    for (ei = efpairs.begin(); ei != efpairs.end(); ++ei) {
      if (ei->second.size() > 2) {
        opts.warning("3 or more faces to an edge");
        break;
      }
    }

    // if geom_save has geometry, part of geom was saved. remerge
    if (geom_save.verts().size())
      geom.append(geom_save);
    // set to merge in any case if duplicates are encountered
    merge_coincident_elements(geom, "vef", opts.epsilon);
  }

  // if coloring new faces, restore color of previous faces
  if ((opts.face_coloring_method == 'o') &&
      (non_color_ops.find(operation) == string::npos)) {
    for (int i = 0; i < (int)geom.faces().size(); i++) {
      Vec3d face_centroid = geom.face_cent(i);
      bool found = false;
      for (int j = 0; j < (int)color_centroids.size(); j++) {
        pair<Vec3d, Color> color_cent = color_centroids[j];
        if (!compare(face_centroid, color_cent.first, opts.epsilon)) {
          geom.colors(FACES).set(i, color_cent.second);
          found = true;
          break;
        }
      }
      if (!found)
        geom.colors(FACES).set(i, opts.map.get_col(operation_number));
    }
  }

  orient_planar(geom, is_orientable, orientation_positive, opts);
}

void do_operations(Geometry &geom, cn_opts &opts)
{
  bool is_orientable = true;
  bool orientation_positive = true;
  int operation_number = 0;

  // the program works better with oriented input, centroid at the origin
  verbose('+', 1, opts);
  GeometryInfo info(geom);
  is_orientable = info.is_orientable();
  if (!is_orientable)
    opts.warning("input file contains a non-orientable geometry. output is "
                 "unpredictable");
  else
    geom.orient(1); // 1=positive

  centroid_to_origin(geom);

  for (auto operation : opts.operations) {
    verbose(operation->op, operation->op_var, opts);

    bool hart_operation_done = false;

    // reflection is done in wythoff
    if (opts.hart_mode) {
      hart_operation_done = true;

      switch (operation->op) {
      // ambo
      case 'a':
        hart_ambo(geom);
        break;

      // gyro
      case 'g':
        hart_gyro(geom);
        break;

      // kis
      case 'k':
        hart_kisN(geom, operation->op_var);
        break;

      // propellor
      case 'p':
        hart_propellor(geom);
        break;

      default:
        hart_operation_done = false;
      }
    }

    if (hart_operation_done) {
      // these steps are needed for hart_mode
      operation_number++;
      orient_planar(geom, is_orientable, orientation_positive, opts);
    }
    else {
      // wythoff mode
      if (opts.alpha_user.find(operation->op) == string::npos)
        wythoff(geom, operation->op, operation->op_var, operation_number,
                is_orientable, orientation_positive, opts);
      else {
        for (auto operation_user : opts.operations_user[operation->op]) {
          verbose(operation_user->op, operation_user->op_var, opts);
          wythoff(geom, operation_user->op, operation_user->op_var,
                  operation_number, is_orientable, orientation_positive, opts);
        }
      }
    }
  }
}

void cn_coloring(Geometry &geom, const cn_opts &opts)
{
  // can't color an empty geom. on -f s it will cause segfault
  if (!geom.verts().size())
    return;

  if (opts.face_coloring_method == 'n') {
    bool trans_success = true;
    const vector<vector<int>> &faces = geom.faces();
    for (unsigned int i = 0; i < faces.size(); i++) {
      int fsz = faces[i].size();
      Color col = opts.map.get_col(fsz - 3);
      if (opts.face_opacity > -1) {
        int opq = (opts.face_pattern[fsz % opts.face_pattern.size()] == '1')
                      ? opts.face_opacity
                      : col[3];
        // map colors always are set but can be map index or invisible
        if (!col.set_alpha(opq))
          trans_success = false;
      }
      geom.colors(FACES).set(i, col);
    }
    if (!trans_success)
      opts.warning("some faces could not be made transparent", 'T');
  }
  else if (opts.face_coloring_method == 's') {
    Symmetry sym;
    vector<vector<set<int>>> sym_equivs;
    sym.init(geom, &sym_equivs);

    Coloring clrng;
    clrng.add_cmap(opts.map.clone());
    clrng.set_geom(&geom);
    clrng.f_sets(sym_equivs[2], true);

    if (opts.face_opacity > -1) {
      ColorValuesToRangeHsva valmap(msg_str("A%g", opts.face_opacity / 255.0));
      valmap.apply(geom, FACES);

      for (const auto &kp : geom.colors(FACES).get_properties()) {
        if (kp.second.is_index()) {
          opts.warning("map indexes cannot be made transparent", 'T');
          break;
        }
      }
    }
  }
  // set color values from map indexes from wythoff call
  else if (opts.face_coloring_method == 'w') {
    Coloring clrng(&geom);
    clrng.add_cmap(opts.map.clone());
    clrng.v_apply_cmap();
    clrng.e_apply_cmap();
    clrng.f_apply_cmap();
  }

  // check if some faces are not set for transparency warning
  if (opts.face_opacity > -1) {
    if (geom.colors(FACES).get_properties().size() < geom.faces().size())
      opts.warning("unset faces cannot be made transparent", 'T');
  }

  // color vertices and edges
  if (opts.face_coloring_method != 'w')
    Coloring(&geom).vef_one_col(opts.vert_col, opts.edge_col, Color());
}

int main(int argc, char *argv[])
{
  cn_opts opts;
  opts.process_command_line(argc, argv);

  Geometry geom;
  if (opts.operand)
    get_operand(geom, opts);
  else
    opts.read_or_error(geom, opts.ifile);

  GeometryInfo info(geom);
  if ((opts.planarize_method == 'p') && !info.is_closed()) {
    opts.planarize_method = 'm';
    opts.warning("input model is not closed. Planarization method p will not "
                 "work. Switching to m");
  }

  do_operations(geom, opts);

  if (opts.unitize)
    unitize_edges(geom);

  cn_coloring(geom, opts);

  opts.write_or_error(geom, opts.ofile);

  verbose('$', 0, opts);

  return 0;
}
