/*
 * Copyright 2015 Scientific Computation Research Center
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */
#include <apfElement.h>

#include "crvAdapt.h"
#include "crvBezier.h"
#include "crvBezierShapes.h"
#include "crvMath.h"
#include "crvQuality.h"
#include "crvTables.h"
#include <maMap.h>
#include <maShapeHandler.h>
#include <maSolutionTransfer.h>
#include <maShape.h>
#include <mth_def.h>
#include <math.h>
#include <cassert>
#include <iostream>
#include <../mds/apfMDS.h>
namespace crv {

class BezierTransfer : public ma::SolutionTransfer
{
  public:
    BezierTransfer(ma::Mesh* m, ma::Refine* r, bool snap)
    {
      mesh = m;
      refine = r;
      shouldSnap = snap;
      // pre compute the inverses of the transformation matrices
      int P = mesh->getShape()->getOrder();
      for (int d = 1; d <= 3; ++d){
        if(!getNumInternalControlPoints(apf::Mesh::simplexTypes[d],P))
          continue;
        int n = getNumControlPoints(apf::Mesh::simplexTypes[d],P);
        mth::Matrix<double> A(n,n);
        Ai[d].resize(n,n);
        getBezierTransformationMatrix(apf::Mesh::simplexTypes[d],P,
            A,elem_vert_xi[apf::Mesh::simplexTypes[d]]);
        invertMatrixWithPLU(getNumControlPoints(apf::Mesh::simplexTypes[d],P),
            A,Ai[d]);
      }
    }
    ~BezierTransfer()
    {
    }
    void getVertParams(int ptype, apf::MeshEntity** parentVerts,
        apf::NewArray<apf::MeshEntity*>& midEdgeVerts, apf::MeshEntity* e,
        apf::Vector3 params[4])
    {
      int npv = apf::Mesh::adjacentCount[ptype][0];
      int ne = apf::Mesh::adjacentCount[ptype][1];
      apf::Downward verts;

      int nv = mesh->getDownward(e,0,verts);
      // first check verts
      for (int v = 0; v < nv; ++v){
        bool vert = false;
        for (int i = 0; i < npv; ++i){
          if(verts[v] == parentVerts[i]){
            params[v] = elem_vert_xi[ptype][i];
            vert = true;
            break;
          }
        }

        // this part relies on "closeness"
        // to determine if this is the correct edge
        if(!vert){
          for (int i = 0; i < ne; ++i){
            if( verts[v] == midEdgeVerts[i] ){
              params[v] = elem_edge_xi[ptype][i];
              break;
            }
          }
        }

      }
    }
    virtual bool hasNodesOn(int dimension)
    {
      return mesh->getShape()->hasNodesIn(dimension);
    }
    virtual void onRefine(
        ma::Entity* parent,
        ma::EntityArray& newEntities)
    {
      int P = mesh->getShape()->getOrder();
      int parentType = mesh->getType(parent);

      // for the parent, get its vertices and mid edge nodes first
      apf::Downward parentVerts,parentEdges;

      mesh->getDownward(parent,0,parentVerts);
      mesh->getDownward(parent,1,parentEdges);
      int ne = apf::Mesh::adjacentCount[parentType][1];

      apf::NewArray<apf::MeshEntity*> midEdgeVerts(ne);
      for (int i = 0; i < ne; ++i)
        midEdgeVerts[i] = ma::findSplitVert(refine,parentEdges[i]);

      int np = getNumControlPoints(parentType,P);

      apf::Element* elem =
          apf::createElement(mesh->getCoordinateField(),parent);
      apf::NewArray<apf::Vector3> nodes;
      apf::getVectorNodes(elem,nodes);
      apf::destroyElement(elem);

      for (size_t i = 0; i < newEntities.getSize(); ++i)
      {
        int childType = mesh->getType(newEntities[i]);
        int ni = mesh->getShape()->countNodesOn(childType);

        if (childType == apf::Mesh::VERTEX || ni == 0 ||
            (mesh->getModelType(mesh->toModel(newEntities[i]))
            < mesh->getDimension() && shouldSnap))
          continue; //vertices will have been handled specially beforehand

        int n = getNumControlPoints(childType,P);

//        apf::Vector3 vp[4];
//        apf::Downward verts;
//        int nv = mesh->getDownward(newEntities[i],0,verts);
//        for (int j = 0; j < ni; ++j){
//          mesh->setPoint(newEntities[i],j,apf::Vector3(0,0,0));
//        }
//        apf::NewArray<apf::Vector3> oldNodes,newNodes(ni);
//        apf::Element* newElem =
//            apf::createElement(mesh->getCoordinateField(),newEntities[i]);
//        apf::getVectorNodes(newElem,oldNodes);
//        apf::destroyElement(newElem);
//
//        for (int v = 0; v < nv; ++v){
//          mesh->getPoint(verts[v],0,vp[v]);
//        }
//        for (int j = 0; j < ni; ++j){
//          apf::Vector3 xi;
//          mesh->getShape()->getNodeXi(childType,j,xi);
//          if(childType == apf::Mesh::EDGE){
//            xi[0] = 0.5*(xi[0]+1.);
//            oldNodes[j+n-ni] = vp[0]*(1.-xi[0])+vp[1]*xi[0];
//          }
//          if(childType == apf::Mesh::TRIANGLE){
//            oldNodes[j+n-ni] = vp[0]*(1.-xi[0]-xi[1])
//                + vp[1]*xi[0] + vp[2]*xi[1];
//          }
//          if(childType == apf::Mesh::TET){
//            oldNodes[j+n-ni] = vp[0]*(1.-xi[0]-xi[1]-xi[2]) + vp[1]*xi[0]
//                + vp[2]*xi[1] + vp[3]*xi[2];
//          }
//          mesh->setPoint(newEntities[i],j,oldNodes[j+n-ni]);
//        }
//        apf::NewArray<double> c;
//        crv::getBezierTransformationCoefficients(P,childType,c);
//        convertInterpolationPoints(n,ni,oldNodes,c,newNodes);
//        for (int j = 0; j < ni; ++j){
//          mesh->setPoint(newEntities[i],j,newNodes[j]);
//        }
        apf::Vector3 vp[4];
        getVertParams(parentType,parentVerts,midEdgeVerts,newEntities[i],vp);

        mth::Matrix<double> A(n,np),B(n,n);
        getBezierTransformationMatrix(parentType,childType,P,A,vp);
        mth::multiply(Ai[apf::Mesh::typeDimension[childType]],A,B);

        for (int j = 0; j < ni; ++j){
          apf::Vector3 point(0,0,0);
          for (int k = 0; k < np; ++k)
            point += nodes[k]*B(j+n-ni,k);

          mesh->setPoint(newEntities[i],j,point);
        }
      }
    }
  private:
    ma::Mesh* mesh;
    ma::Refine* refine;
    mth::Matrix<double> Ai[4];
    bool shouldSnap;
};

typedef std::map<ma::Entity*,ma::Entity*> ES;
typedef std::map<ma::Entity*,ma::Entity*>::iterator ESIt;

class BezierHandler : public ma::ShapeHandler
{
  public:
    BezierHandler(ma::Adapt* a)
    {
      mesh = a->mesh;
      bt = new BezierTransfer(mesh,a->refine,a->input->shouldSnap);
      sizeField = a->sizeField;
      shouldSnap = a->input->shouldSnap;
    }
    ~BezierHandler()
    {
      delete bt;
    }
    virtual double getQuality(apf::MeshEntity* e)
    {
      assert( mesh->getType(e) == apf::Mesh::TRIANGLE ||
          mesh->getType(e) == apf::Mesh::TET);
      return ma::measureElementQuality(mesh, sizeField, e)
        *crv::getQuality(mesh,e);
    }
    virtual bool hasNodesOn(int dimension)
    {
      return bt->hasNodesOn(dimension);
    }
    virtual void onRefine(
        apf::MeshEntity* parent,
        ma::EntityArray& newEntities)
    {
      bt->onRefine(parent,newEntities);
    }
//    void getHull(ma::EntityArray& cavity,
//        ES& hull)
//    {
//      int md = mesh->getDimension();
//      apf::Downward down;
//      for (size_t i = 0; i < cavity.getSize(); ++i){
//        int nd = mesh->getDownward(cavity[i],md-1,down);
//        for (int j = 0; j < nd; ++j){
//          ESIt it = hull.find(down[j]);
//          if(it == hull.end())
//            hull[down[j]] = cavity[i];
//          else
//            hull.erase(it);
//        }
//      }
//    }
//    apf::Vector3 getDirection(ma::Entity* vert, ma::Entity* edge)
//    {
//      apf::Vector3 node, vertPosition = ma::getPosition(mesh,vert);
//      apf::Downward verts;
//      mesh->getDownward(edge,0,verts);
//      if(verts[0] == vert){
//        mesh->getPoint(edge,0,node);
//        return node - vertPosition;
//      } else {
//        int lastNode = mesh->getShape()->countNodesOn(apf::Mesh::EDGE)-1;
//        mesh->getPoint(edge,lastNode,node);
//        return node - vertPosition;
//      }
//    }
    // for a given edge and a cavity, attempt to find the pair
    // of triangles that the edge spans. This is not always possible
    bool findEdgeTrianglesCross(ma::EntityArray& entities,
        ma::Entity* edgeVerts[2],
        ma::Entity* edgeTris[2])
    {
      ma::Entity* edge0 = 0;
      ma::Entity* edge1 = 0;
      for (size_t i = 0; i < entities.getSize(); ++i){
        if (mesh->getType(entities[i]) != apf::Mesh::TRIANGLE) continue;
        if (ma::isInClosure(mesh,entities[i],edgeVerts[0])){
          // try this edge, see if theres a match opposite it
          edge0 = ma::getTriEdgeOppositeVert(mesh,entities[i],edgeVerts[0]);
          for (size_t j = 0; j < entities.getSize(); ++j){
            if(j == i) continue;
            if(ma::isInClosure(mesh,entities[j],edgeVerts[1])){
              edge1 = ma::getTriEdgeOppositeVert(mesh,entities[j],edgeVerts[1]);
              if(edge0 == edge1){
                edgeTris[0] = entities[i];
                edgeTris[1] = entities[j];
                return true;
              }
            }
          }
        }
      }
      return false;
    }
    // for a given edge and a cavity, attempt to find the pair
    // of triangles that share the edge
    // by construction, these must be all in the cavity
    // should do something more intelligent in 3D
    bool findEdgeTrianglesShared(ma::Entity* edge,
        ma::Entity* edgeTris[2])
    {
      apf::Up up;
      mesh->getUp(edge,up);
      edgeTris[0] = up.e[0];
      edgeTris[1] = up.e[1];
      assert(ma::isInClosure(mesh,edgeTris[0],edge));
      assert(ma::isInClosure(mesh,edgeTris[1],edge));
      return true;
    }
    void evaluateBlendedQuad(ma::Entity* verts[4], ma::Entity* edges[4],
        int dir[4], apf::Vector3& xi, apf::Vector3& point)
    {
      point.zero();
      apf::Vector3 pt;
      apf::Vector3 xii[4] = {
          apf::Vector3(2.*xi[0]-1.,0,0),apf::Vector3(2.*xi[1]-1.,0,0),
          apf::Vector3(1.-2.*xi[0],0,0),apf::Vector3(1.-2.*xi[1],0,0)};
      double eC[4] = {1.-xi[1],xi[0],xi[1],1.-xi[0]};
      double vC[4] = {eC[3]*eC[0],eC[0]*eC[1],eC[1]*eC[2],eC[2]*eC[3]};
      for (int i = 0; i < 4; ++i){
        apf::Element* elem =
            apf::createElement(mesh->getCoordinateField(),edges[i]);
        if(!dir[i]) xii[i][0] *= -1.;
        apf::getVector(elem,xii[i],pt);
        point += pt*eC[i]-ma::getPosition(mesh,verts[i])*vC[i];
        apf::destroyElement(elem);
      }
    }

    void setBlendedQuadEdgePoints(ma::Entity* edge,
        ma::Entity* verts[4], ma::Entity* edges[4],
        int dir[4])
    {
      int P = mesh->getShape()->getOrder();
      if(P == 2){
        apf::Vector3 xi(0.5,0.5,0);
        apf::Vector3 point;
        evaluateBlendedQuad(verts,edges,dir,xi,point);
        mesh->setPoint(edge,0,point);
      } else {
        apf::Vector3 xi(1./3.,1./3.,0);
        apf::Vector3 point;
        evaluateBlendedQuad(verts,edges,dir,xi,point);
        mesh->setPoint(edge,0,point);
        xi[0] = 2./3.; xi[1] = 2./3.;
        evaluateBlendedQuad(verts,edges,dir,xi,point);
        mesh->setPoint(edge,1,point);
        if (P > 3)
          elevateBezierCurve(mesh,edge,3,P-3);
      }
    }
    ma::Entity* findEdgeInTri(ma::Entity* v0, ma::Entity* v1,
        ma::Entity* tri, int& dir)
    {
      ma::Entity* edges[3];
      mesh->getDownward(tri,1,edges);
      for (int i = 0; i < 3; ++i){
        ma::Entity* verts[2];
        mesh->getDownward(edges[i],0,verts);
        if(verts[0] == v0 && verts[1] == v1){
          dir = 1;
          return edges[i];
        }
        if(verts[0] == v1 && verts[1] == v0){
          dir = 0;
          return edges[i];
        }
      }
      fail("can't find edge in tri");
    }
    bool setBlendedQuadEdgePointsOld(ma::EntityArray& cavity,
        ma::Entity* edge)
    {
      ma::Entity* edgeVerts[2];
      ma::Entity* edgeTris[2];
      mesh->getDownward(edge,0,edgeVerts);
      if(findEdgeTrianglesCross(cavity,edgeVerts,edgeTris)){
        int index[4];
        ma::Entity* vA[3], *vB[3];
        mesh->getDownward(edgeTris[0],0,vA);
        mesh->getDownward(edgeTris[1],0,vB);
        index[0] = apf::findIn(vA,3,edgeVerts[0]);
        index[1] = apf::findIn(vB,3,vA[(index[0]+1) % 3]);
        index[2] = apf::findIn(vB,3,edgeVerts[1]);
        index[3] = (index[0]+2) % 3;

        ma::Entity* verts[4] = {vA[index[0]],vB[index[1]],
            vB[index[2]],vA[index[3]]};

        ma::Entity* edges[4];
        int dir[4];
        for (int i = 0; i < 4; ++i)
          edges[i] = findEdgeInTri(verts[i],verts[(i+1)%4],
              edgeTris[i==1||i==2],dir[i]);

        setBlendedQuadEdgePoints(edge,verts,edges,dir);
        return true;
      }
      return false;
    }
    void setBlendedQuadEdgePointsNew(ma::Entity* edge)
    {
      ma::Entity* edgeVerts[2];
      ma::Entity* edgeTris[2];
      mesh->getDownward(edge,0,edgeVerts);
      findEdgeTrianglesShared(edge,edgeTris);
      int index[4];
      ma::Entity* vA[3], *vB[3];
      mesh->getDownward(edgeTris[0],0,vA);
      mesh->getDownward(edgeTris[1],0,vB);
      index[0] = apf::findIn(vA,3,edgeVerts[0]);
      index[1] = apf::findIn(vA,3,
          ma::getTriVertOppositeEdge(mesh,edgeTris[0],edge));
      index[2] = apf::findIn(vB,3,edgeVerts[1]);
      index[3] = apf::findIn(vB,3,
          ma::getTriVertOppositeEdge(mesh,edgeTris[1],edge));
      assert(index[0] >= 0 && index[1] >= 0);
      assert(index[2] >= 0 && index[3] >= 0);
      ma::Entity* verts[4] = {vA[index[0]], vA[index[1]],
          vB[index[2]],vB[index[3]]};
      ma::Entity* edges[4];
      int dir[4];
      for (int i = 0; i < 4; ++i)
        edges[i] = findEdgeInTri(verts[i],verts[(i+1)%4],edgeTris[i>1],dir[i]);

      setBlendedQuadEdgePoints(edge,verts,edges,dir);
    }
    void setLinearEdgePoints(ma::Entity* edge)
    {
      apf::Vector3 xi,points[2];
      apf::MeshEntity* verts[2];
      int ni = mesh->getShape()->countNodesOn(apf::Mesh::EDGE);
      mesh->getDownward(edge,0,verts);
      mesh->getPoint(verts[0],0,points[0]);
      mesh->getPoint(verts[1],0,points[1]);
      for (int j = 0; j < ni; ++j){
        mesh->getShape()->getNodeXi(apf::Mesh::EDGE,j,xi);
        double t = (1.+j)/(1.+ni);
        xi = points[0]*(1.-t)+points[1]*t;
        mesh->setPoint(edge,j,xi);
      }
    }
    virtual void onCavity(
        ma::EntityArray& oldElements,
        ma::EntityArray& newEntities)
    {

      apf::FieldShape* fs = mesh->getShape();
      int P = fs->getOrder();
      // deal with all the boundary points, if a boundary edge has been
      // collapsed, this is a snapping operation
      int n = fs->getEntityShape(apf::Mesh::EDGE)->countNodes();
      apf::NewArray<double> c;
      crv::getBezierTransformationCoefficients(P,1,c);
      for (size_t i = 0; i < newEntities.getSize(); ++i)
      {
        int newType = mesh->getType(newEntities[i]);
        int ni = mesh->getShape()->countNodesOn(newType);
        if (newType != apf::Mesh::EDGE) continue;

        if (mesh->getModelType(mesh->toModel(newEntities[i]))
            < mesh->getDimension() && ni > 0 && shouldSnap){
            snapToInterpolate(mesh,newEntities[i]);
            convertInterpolationPoints(mesh,newEntities[i],n,ni,c);
        }
      }
      ma::EntityArray middleEdges;
      for (size_t i = 0; i < newEntities.getSize(); ++i){
        if(mesh->getType(newEntities[i]) != apf::Mesh::EDGE) continue;
        if(mesh->getModelType(mesh->toModel(newEntities[i]))
            < mesh->getDimension()) continue;
        if(!setBlendedQuadEdgePointsOld(oldElements,newEntities[i])){
          middleEdges.append(newEntities[i]);
          // set to linear, because we don't know any better
          setLinearEdgePoints(newEntities[i]);
        }
      }
      for (size_t i = 0; i < middleEdges.getSize(); ++i)
        setBlendedQuadEdgePointsNew(middleEdges[i]);

      for (int d = 2; d <= mesh->getDimension(); ++d){

        int ni = fs->countNodesOn(apf::Mesh::simplexTypes[d]);
        if(ni == 0) continue;

        n = fs->getEntityShape(apf::Mesh::simplexTypes[d])->countNodes();
        apf::NewArray<double> c;
        crv::getInternalBezierTransformationCoefficients(mesh,
            P,1,apf::Mesh::simplexTypes[d],c);

        for (size_t i = 0; i < newEntities.getSize(); ++i)
        {
          int newType = mesh->getType(newEntities[i]);
          // zero the newEntities.
          if(apf::Mesh::typeDimension[newType] == d){

            for (int j = 0; j < ni; ++j){
              apf::Vector3 zero(0,0,0);
              mesh->setPoint(newEntities[i],j,zero);
            }
          }
          if (apf::Mesh::typeDimension[newType] == d && ni > 0
              && (mesh->getModelType(mesh->toModel(newEntities[i]))
              == mesh->getDimension() || !shouldSnap)){
            convertInterpolationPoints(mesh,newEntities[i],n-ni,ni,c);
          }
        }
      }
    }
  private:
    ma::Mesh* mesh;
    BezierTransfer* bt;
    ma::SizeField * sizeField;
    bool shouldSnap;
};

ma::ShapeHandler* getShapeHandler(ma::Adapt* a)
{
  return new BezierHandler(a);
}

}