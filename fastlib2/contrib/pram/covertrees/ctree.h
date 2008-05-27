#ifndef TREE_COVER_TREE_H
#define TREE_COVER_TREE_H

#include <fastlib/fastlib.h>

#include "cover_tree.h"
#include "distances.h"

namespace ctree {

  template<typename T> 
  T BASE() {
    T BASE = 1.3;
    return BASE;
  }

  template<typename T> 
  T inverse_log_base() {
    T inverse_log_base  = 1.0 / log(BASE<T>());
    return inverse_log_base;
  }

  const index_t NEG_INF = (int) log(0);

  template<typename T>
  inline T scaled_distance(index_t scale) {
    return pow(BASE<T>(), scale);
  }

  template<typename T>
  inline index_t scale_of_distance(T distance) {
    return (index_t) ceil(log(distance)*inverse_log_base<T>());
  }

  template<typename T>
  class NodeDistances {
    
  private:
    // the point not needed, we just use the index when we 
    // make an arraylist for each of the point
    // Vector point_;
    index_t point_;
    ArrayList<T> distances_;
    
  public:
    
    NodeDistances() {
      distances_.Init(0);
    }
    
    ~NodeDistances() {
    }

    index_t point() {
      return point_;
    }

    ArrayList<T> *distances() {
      return &distances_;
    }

    T distances(index_t in) {
      return distances_[in];
    }

    void add_distance(T dist) {
      distances_.PushBackCopy(dist);
      return;
    }

    void Init(index_t point, T dist) {
      point_ = point;
      distances_.PushBackCopy(dist);
      return;
    }
  };
  
  template<typename T>
  T max_set(ArrayList<NodeDistances<T>*> *set) {

    T max = 0.0;
    for (index_t i = 0; i < set->size(); i++) {
      if(max < (*set)[i]->distances()->back()) {
	max = (*set)[i]->distances()->back();
      }
    }
    return max;
  }

  void print_space(index_t n) {
    for (index_t i = 0; i < n; i++) {
      printf("\t");
    }
    return;
  }

  // find out how to print templatized things
  template<typename TCoverTreeNode>
  void print_tree(index_t depth, TCoverTreeNode *top_node) {
    print_space(depth);
    printf("Point %"LI"d:", top_node->point()+1);
    //NOTIFY("%"LI"d:%"LI"d", top_node->point()+1, top_node->scale_depth());
    if (top_node->num_of_children() > 0) {
      printf("scale_depth = %"LI"d, max_dist = %lf, children = %"LI"d\n",
           top_node->scale_depth(), top_node->max_dist_to_grandchild(),
           top_node->num_of_children());
      for (index_t i = 0; i < top_node->num_of_children(); i++) {
	print_tree(depth+1, top_node->child(i));
      }
    }
    else {
      printf("\n");
    }
    return;
  }

  template<typename TCoverTreeNode>
  void PrintTree(TCoverTreeNode *top_node) {
    index_t depth = 0;
    print_tree(depth, top_node);
    return;
  }

  // here we assume that both the point_set and the far set are
  // already initialized
  template<typename T>
  void split_far(ArrayList<NodeDistances<T>*> *point_set, 
		 ArrayList<NodeDistances<T>*> *far,
		 index_t scale) {

    T bound = scaled_distance<T>(scale);
    index_t initial_size = far->size();
    ArrayList<NodeDistances<T>*> near;
    NodeDistances<T> **begin = point_set->begin();
    NodeDistances<T> **end = point_set->end();

    near.Init(0);
    for (; begin < end; begin++) {
      if ((*begin)->distances()->back() > bound) {
	far->PushBackCopy(*begin);
      }
      else {
	near.PushBackCopy(*begin);
      }
    }

    DEBUG_ASSERT_MSG(point_set->size() == 
		     far->size() - initial_size + near.size(), 
		     "split_far: point set size doesn't add up\n");

    point_set->Renew();
    point_set->InitSteal(&near);

    return;
  }

  // here we assume that the point_set and the near set are 
  // already initialized
  template<typename T>
  void split_near(index_t point, const GenMatrix<T>& data,
		  ArrayList<NodeDistances<T>*> *point_set,
		  ArrayList<NodeDistances<T>*> *near,
		  index_t scale) {

    T bound = scaled_distance<T>(scale);
    index_t initial_size = near->size();
    ArrayList<NodeDistances<T>*> far;
    NodeDistances<T> **begin = point_set->begin();
    NodeDistances<T> **end = point_set->end();
    GenVector<T> p;

    data.MakeColumnVector(point, &p);
    far.Init(0);
    for (; begin < end; begin++) {

      GenVector<T> q;

      data.MakeColumnVector((*begin)->point(), &q);
      //T dist = sqrt(la::DistanceSqEuclidean(p,q));
      T dist = pdc::DistanceEuclidean<T>(p, q, bound);
      if (dist > bound) {
	far.PushBackCopy(*begin);
      }
      else {
	(*begin)->add_distance(dist);
	near->PushBackCopy(*begin);
      }
    }

    DEBUG_ASSERT_MSG(point_set->size() == 
		     near->size() - initial_size + far.size(),
		     "split_near: point set doesn't add up\n");

    point_set->Renew();
    point_set->InitSteal(&far);

    return;
  }

  template<typename TCoverTreeNode, typename T>
  TCoverTreeNode *private_make_tree(index_t point, const GenMatrix<T>& data,
				    index_t current_scale,
				    index_t max_scale, 
				    ArrayList<NodeDistances<T>*> *point_set,
				    ArrayList<NodeDistances<T>*> *consumed_set) {
    
    // no other point so leaf in explicit tree
    if (point_set->size() == 0) { 
      TCoverTreeNode *node = new TCoverTreeNode();
      node->MakeLeafNode(point);
      return node;
    }
    else {
      T max_dist = max_set(point_set);
      index_t next_scale = min(current_scale - 1, scale_of_distance(max_dist));
      
      // At the -INF level so all points are nodes
      // and we have point with zero distances
      if (next_scale == NEG_INF) { 
	ArrayList<TCoverTreeNode*> children;
	NodeDistances<T> **begin = point_set->begin();
	NodeDistances<T> **end = point_set->end();

	children.Init(0);
	children.PushBack()->MakeLeafNode(point);

	for (; begin < end; begin++) {
	  children.PushBack()->MakeLeafNode((*begin)->point());
	  consumed_set->PushBackCopy(*begin);
	}

	DEBUG_ASSERT(children.size() == point_set->size());
	point_set->Resize(0);
	TCoverTreeNode *node = new TCoverTreeNode();
	node->MakeNode(point, 0.0, 100, &children);

	return node;
      }

      // otherwise you need to recurse
      else {

	ArrayList<NodeDistances<T>*> far;

	far.Init(0);
	split_far(point_set, &far, current_scale);
	
	TCoverTreeNode *child = 
	  private_make_tree<TCoverTreeNode>(point, data, next_scale, 
					    max_scale, point_set, 
					    consumed_set);
	
	if (point_set->size() == 0) {
	  point_set->Renew();
	  point_set->InitSteal(&far);

	  return child;
	}

	else {

	  ArrayList<TCoverTreeNode*> children;
	  ArrayList<NodeDistances<T>*> new_point_set, new_consumed_set;

	  children.Init(0);
	  new_point_set.Init(0);
	  new_consumed_set.Init(0);
	  children.PushBackCopy(child);
	  //NOTIFY("%"LI"d:%"LI"d", child->point()+1, child->scale_depth());

	  while (point_set->size() != 0) {

	    index_t new_point;
	    T new_dist = point_set->back()->distances()->back();

	    new_point = point_set->back()->point();
	    // remember to check here what to use, PushBackRaw() or AddBack()
	    // so that we can use PopBackInit(Element *dest)
	    point_set->PopBackInit(consumed_set->PushBackRaw()); 

	    split_near(new_point, data, point_set, 
		       &new_point_set, current_scale);
	    split_near(new_point, data, &far, 
		       &new_point_set, current_scale);
	    
	    TCoverTreeNode *child_node = 
	      private_make_tree<TCoverTreeNode>(new_point, data,
						next_scale,max_scale,
						&new_point_set,
						&new_consumed_set);
	    
	    child_node->set_dist_to_parent(new_dist);
	    children.PushBackCopy(child_node);
	    //NOTIFY("%"LI"d:%"LI"d", child_node->point()+1, child_node->scale_depth());
	    
	    T bound = scaled_distance<T>(current_scale);
	    NodeDistances<T> **begin = new_point_set.begin();
	    NodeDistances<T> **end = new_point_set.end();

	    for (; begin < end; begin++) {

	      (*begin)->distances()->PopBack();
	      if ((*begin)->distances()->back() <= bound) {
		point_set->PushBackCopy(*begin);
	      }
	      else {
		far.PushBackCopy(*begin);
	      }
	    }
	    new_point_set.Resize(0);
	    
	    while (new_consumed_set.size() > 0) {
	      new_consumed_set.back()->distances()->PopBack();
	      new_consumed_set.PopBackInit(consumed_set->PushBackRaw());
	    }
	   
	  }

	  point_set->Renew();
	  point_set->InitSteal(&far);

	  TCoverTreeNode *node = new TCoverTreeNode();

	  node->MakeNode(point, max_set(consumed_set), 
			 max_scale - current_scale, &children);
	  
	  return node;
	}
      }
    }
  }

  template<typename TCoverTreeNode, typename T>
  TCoverTreeNode *MakeCoverTree(const GenMatrix<T>& dataset) {

    index_t n = dataset.n_cols();
    DEBUG_ASSERT(n > 0);
    ArrayList<NodeDistances<T>*> point_set, consumed_set;
    GenVector<T> root_point;

    dataset.MakeColumnVector(0, &root_point);
    point_set.Init(0);
    consumed_set.Init(0);

    // speed up possible here by using pointers
    for (index_t i = 1; i < n; i++) {
      NodeDistances<T> *node_distances = new NodeDistances<T>();
      GenVector<T> point;
      T dist;

      dataset.MakeColumnVector(i, &point);
      //dist = sqrt(la::DistanceSqEuclidean(root_point, point));
      dist = pdc::DistanceEuclidean<T>(root_point, point, sqrt(DBL_MAX));

      node_distances->Init(i, dist);

      point_set.PushBackCopy(node_distances);
    }
    DEBUG_ASSERT(point_set.size() == n - 1);

    T max_dist = max_set(&point_set);
    index_t max_scale = scale_of_distance(max_dist);
    
    
    TCoverTreeNode *root_node = 
      private_make_tree<TCoverTreeNode, T>(0, dataset, max_scale,
					   max_scale, &point_set,
					   &consumed_set);
    
    return root_node;
  }

};
#endif
