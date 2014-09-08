#ifndef PARMA_TARGETS_H
#define PARMA_TARGETS_H
#include <apfMesh.h>
#include "parma_associative.h"

namespace parma {
  class Sides;
  class Weights;
  class Ghosts;
  class Targets : public Associative<double> {
    public:
      Targets(Sides*, Weights*, double) {}
      virtual ~Targets() {}
      virtual double total()=0;
    private:
      Targets();
  };
  Targets* makeTargets(Sides* s, Weights* w, double alpha);
  Targets* makeShapeTargets(apf::Mesh* m, Sides* s, Weights* w, double alpha);
  Targets* makeGhostTargets(Sides* s, Weights* w, Ghosts* g, double alpha);
}
#endif
