#ifndef INCLUDED_RANDOM_FORESTS_HPP
#define INCLUDED_RANDOM_FORESTS_HPP

#include <cstdlib>
#include <iostream>
#include <fstream>

#include <string>
#include <vector>
#include <map>
#include <queue>
#include <ext/algorithm>

#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/random.hpp>
#include <boost/multi_array.hpp>
#include <boost/lambda/lambda.hpp>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/utility.hpp> //for pair
#include <boost/serialization/export.hpp>
#include <boost/serialization/base_object.hpp>


using namespace std;
using namespace boost;
using namespace boost::lambda;
using namespace boost::algorithm;
using namespace boost::archive;

#define foreach BOOST_FOREACH

typedef pair<int, vector<int> > pivi;
typedef double (*eval_func)(double);

mt19937 mt(static_cast<uint32_t>(time(0)));
static random_number_generator<mt19937> rng( mt );

double gini( double p ){
  return p * (1.0 - p);
}

vector<int> range_vector(int len){
  vector<int> result(len);
  int val(0);
  generate_n(result.begin(),len, var(val)++);
  return result;
}

class Sample{
public:
  int index;
  vector<double> data;
  int cls;
  string name;
		
  Sample():index(-1),cls(0),name(""){}

private:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const unsigned int version)
  {
    ar & index;
    ar & data;
    ar & cls;
    ar & name;
  }

};

/*
 * we need header?
 */
typedef pair< vector<Sample*>, vector<Sample*> > pvsvs;

class SampleSet : public vector<Sample>{
public :
  vector<string> attr_list;
  // # of cls type
  int cls_type;
  // all kind of classes
  set<int> cls_ids; 
  // for convinience
  int attr_len;
  int sample_len;
  
  SampleSet(){}

  SampleSet( const string& data_file , const string& cls_file , const string& attr_list_file ):cls_type(0),attr_len(0),sample_len(0) {
    //read attributes for restrict (if needs)
    vector<string> attrs(0);
    string line;
    if( attr_list_file != "" ){
      ifstream infile(attr_list_file.c_str() , fstream::in);
      while(getline(infile,line)){
	attrs.push_back(line);
      }
      infile.close();
    }
    //read-in cls file
    ifstream infile(cls_file.c_str() , fstream::in);
    map<string,int> _cls;// = {} # { sample_name : cls }
    vector<string> _temp;
    while(getline(infile,line)){
      split( _temp , line , is_any_of("\t") );
      int _cls_val =  atoi( _temp[1].c_str() );
      if( _cls_val == 0 ){
	cout << "You can't use 0 as class identifier. Use other integer (negative int is also permitted.)" << endl;
	exit(1);
      }
      _cls[_temp[0]] = _cls_val;
      cls_ids.insert(_cls_val);
    }
    infile.close();
    cls_type = cls_ids.size();
    infile.open( data_file.c_str() );
    vector<string> _sample_name;
    getline(infile,line);
    split( _sample_name, line, is_any_of("\t"));
    _sample_name.erase( _sample_name.begin() );
    if( _sample_name.size() != _cls.size()){
      cout << "sample size inconsistency!" << endl;
      exit (1);
    }
    for( int i = 0; i != _sample_name.size(); ++i){
      Sample s;
      s.index = i;
      s.name = _sample_name[i];
      map<string,int>::iterator itr = _cls.find( _sample_name[i] );
      if(itr == _cls.end() ){
	cout << "name no match :" << _sample_name[i] << endl;
	exit(1);
      }
      s.cls = _cls[_sample_name[i]];
      this->push_back( s );
    }
    while(getline(infile,line)){
      split( _temp, line, is_any_of("\t") );
      if(attrs.size() != 0 && (attrs.end() == find(attrs.begin(),attrs.end(),_temp[0])) ) continue;
      attr_list.push_back( _temp[0] );
      _temp.erase( _temp.begin() );
      for(int i=0; i!=_temp.size();i++ ){
	this->at(i).data.push_back( atof( _temp[i].c_str() ) );
      }
    }
    infile.close();
    
    sample_len = this->size();
    attr_len = attr_list.size();
  }  

  pvsvs get_random_list_balanced( ){
    vector<int> size_vec( int_rand_div( ) );
    map<int,vector<int> > bag;//{cls:[sample index...]
    vector<Sample*> oob;
    vector<Sample*> result;
    set<int> cls_set;
    foreach( Sample s , *this ){
      cls_set.insert(s.cls);
      map<int,vector<int> >::iterator it = bag.find( s.cls );
      if( it == bag.end() ){
	vector<int> _vec;
	_vec.push_back(s.index);
	bag[s.cls] = _vec;
      }else {
	it->second.push_back(s.index);
      }
    }
    if(cls_set.size() != 2){
      cout << "class size must be 2" << endl;
      exit(1);
    }
    vector<int> cls_vec;
    foreach( int _cls, cls_set ){
      cls_vec.push_back(_cls);
    }

    vector<int> oob_index(range_vector(sample_len));
    //which is the minor class?
    int minor_size = 0;
    int major_size = 0;
    int minor_cls = 0;
    int major_cls = 0;
    if( bag[cls_vec[0]].size() > bag[cls_vec[1]].size() ){
      minor_cls = cls_vec[1];
      major_cls = cls_vec[0];
    }else {
      minor_cls = cls_vec[0];
      major_cls = cls_vec[1];
    }      
    minor_size = bag[minor_cls].size();
    major_size = sample_len - minor_size;
    
#ifndef NDEBUG
    cout << "minor_cls:" << minor_cls << endl;
    cout << "major_cls:" << major_cls << endl;
#endif

    // bootstrapping minor class samples
    for( int i=0; i != minor_size; ++i ){
      int index = rng(minor_size);
      result.push_back( &(this->at(bag[minor_cls][index])) );
#ifndef NDEBUG
    cout << "for minor cls add :" << bag[minor_cls][index] << endl;
#endif
      vector<int>::iterator itr = find( oob_index.begin() , oob_index.end() , bag[minor_cls][index] );
      if( itr != oob_index.end() ) oob_index.erase( itr );
    }
    int minor_selected_size = result.size();
    for( int i=0; i != minor_selected_size; ++i ){
      int index = rng(major_size);
#ifndef NDEBUG
    cout << "for major cls add :" << bag[major_cls][index] << endl;
#endif
      result.push_back( &(this->at(bag[major_cls][index])) );
      vector<int>::iterator itr = find( oob_index.begin() , oob_index.end() , bag[major_cls][index] );
      if( itr != oob_index.end() ) oob_index.erase( itr );
    }
    foreach( int i, oob_index ){
#ifndef NDEBUG
      cout << "for oob:" << i << endl;
#endif
      oob.push_back( &(this->at(i)) );
    }
    return make_pair( result , oob );
  }

  pvsvs get_random_list_weighted( ){
    vector<int> size_vec( int_rand_div( ) );
    map<int,vector<int> > bag;//{cls:[sample index...]
    vector<Sample*> oob;
    vector<Sample*> result;

    foreach( Sample s , *this ){
      map<int,vector<int> >::iterator it = bag.find( s.cls );
      if( it == bag.end() ){
	vector<int> _vec;
	_vec.push_back(s.index);
	bag[s.cls] = _vec;
      }else {
	it->second.push_back(s.index);
      }
    }
    vector<int> oob_index(range_vector(sample_len));
    
    int size_index = 0;
    
    foreach( pivi v, bag ){
      for( int i=0; i != size_vec[size_index]; ++i ){
	int index = rng(v.second.size());
	result.push_back( &(this->at(v.second[index])) );
	vector<int>::iterator itr = find( oob_index.begin() , oob_index.end() , v.second[index] );
	if( itr != oob_index.end() ) oob_index.erase( itr );
      }
      ++size_index;
    }
    foreach( int i, oob_index ){
      oob.push_back( &(this->at(i)) );
    }
    return make_pair( result , oob );
  }
  
  pvsvs get_random_list( ){
    vector<Sample*> oob;
    vector<Sample*> result;
    vector<int> oob_index( range_vector(sample_len));

    for( int i=0; i != sample_len; ++i ){
      int index = rng(sample_len);
      result.push_back( &(this->at(index)) );
      vector<int>::iterator itr = find( oob_index.begin() , oob_index.end() , index );
      if( itr != oob_index.end() ) oob_index.erase( itr );
    }
    foreach( int i, oob_index ){
      //cout << i << ",";
      oob.push_back( &(this->at(i)) );
    }
    //cout << endl;
    return make_pair( result , oob );
  }

  vector<double> get_random_perm_data( const vector<Sample*>& oob, int attr_index ){
    vector<double> result;
    foreach( Sample *s, oob ){
      result.push_back( s->data[attr_index] );
    }
    random_shuffle( result.begin(), result.end(), rng );
    return result;
  }

private:
  vector<int> int_rand_div( ){
    vector<int> div_vec(cls_type);
    int base = sample_len / cls_type;
    fill_n( div_vec.begin(), cls_type, base);
    int rem = sample_len % cls_type;
    if( rem == 0 ) return div_vec;
    vector<int> vec(range_vector(cls_type));
    vector<int> rand_index(rem);
    random_sample_n( vec.begin() , vec.end() , rand_index.begin() , rem , rng );    
    foreach( int i, rand_index){
      ++div_vec[i];
    }
    return div_vec;
  }

  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const unsigned int version)
  {
    ar & boost::serialization::base_object< vector<Sample> >(*this);
    ar & attr_list;
    ar & cls_type;
    ar & attr_len;
    ar & sample_len;
  }


};

class Node{
  friend ostream& operator<<(ostream& out, const Node& obj);
public:
  int parent_index;
  int right_child;
  int left_child;
  int div_attr;//index of attribute list
  double div_value;
  vector<Sample*> samples;
  bool is_leaf;

  Node():parent_index(-1),right_child(-1),left_child(-1),div_attr(-1),div_value(0.0),is_leaf(false){}

  int size() const {
    return samples.size();
  }

  void set_data( vector<Sample*> _data_set ){
    samples = _data_set;
    if(size() == 0){
      is_leaf = false;
      return ;
    }
    set<int> _temp;
    foreach( Sample *v, samples){
      _temp.insert(v->cls);
    }
    if( _temp.size() == 1 ) is_leaf = true;
  }
  
  int get_cls() const {
    return is_leaf ? samples[0]->cls : 0;
  }

private:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const unsigned int version)
  {
    ar & parent_index;
    ar & right_child;
    ar & left_child;
    ar & div_attr;//index of attribute list
    ar & div_value;
    ar & samples;
    ar & is_leaf;
  }
};

ostream& operator<<(ostream& out, const Node& obj){
  out << "pid=" << obj.parent_index << endl
      << " : rid=" << obj.right_child << " : lid=" << obj.left_child << endl
      << " : div_attr=" << obj.div_attr << " : div_value=" << obj.div_value << endl
      << " : cls=" << obj.get_cls() << endl;
  foreach( Sample *s, obj.samples ){
    out << s->name << "\t";
  }
  out << endl;
  return(out);
}

class Tree : public vector<Node>{
  friend ostream& operator<<(ostream& out, const Tree& obj);
  
public:
  // random sampled samples and oob set (if exists)
  pvsvs materials;

  Tree( pvsvs _data ){
    materials = _data;
  }

  Tree(){}

  const vector<int> get_div_attr_list(){
    set<int> _result;
    foreach( Node n , *this ){
      if( n.div_attr != -1 ) _result.insert( n.div_attr );
    }
    vector<int> result(_result.begin(),_result.end());
    return result;
  }
  
  const map<int,int> get_div_attr_and_size_list(){
    map<int,int> d;
    foreach( Node n , *this ){
      if( n.div_attr != -1){
	d[n.div_attr] += n.samples.size();
	//int _num = d[n.div_attr];
	//if( _num < n.samples.size() ) d[n.div_attr] = _num;
      }
    }
    return d;
  }
  
  int classify( Sample& s ) const {
    int index(0);
    while(true){
      if( this->at(index).is_leaf) break;
      else {
	if( s.data[ this->at(index).div_attr ] < this->at(index).div_value ){
	  index = this->at(index).left_child;
	} else {
	  index = this->at(index).right_child;
	}
      }
    }
    return index;
  }
  
  int classify( Sample& s, int perm_index, double perm_val ) const {
    int index(0);
    while(true){
      if( this->at(index).is_leaf) break;
      else {
	double val(0.0);
	if( this->at(index).div_attr == perm_index ){
	  val = perm_val;
	}else {
	  val = s.data[ this->at(index).div_attr ];
	}
	if( val < this->at(index).div_value ){
	  index = this->at(index).left_child;
	} else {
	  index = this->at(index).right_child;
	}
      }
    }
    return index;
  }

  bool judge( Sample &s ) const {
    int index = classify( s );
    return s.cls == this->at(index).get_cls();
  }

  bool judge( Sample &s, int perm_index, double perm_val ) const {
    int index = classify( s, perm_index, perm_val );
    return s.cls == this->at(index).get_cls();
  }

  void update_prox_mat( multi_array<double,2>& mat, vector<Sample>& all_data, vector<Sample*>& _oob){
    vector<int> pos_list(all_data.size());
    for( int i = 0; i != this->size(); ++i){
      if( this->at(i).is_leaf ){
	foreach( Sample* s, this->at(i).samples){
	  pos_list[s->index] = i;
	}
      }
    }
    foreach( Sample *s, _oob ){
      pos_list[s->index] = classify(*s);
    }
    for( int i = 0; i != all_data.size(); ++i){
      for( int j = 0; j != all_data.size(); ++j){
	if( pos_list[i] == pos_list[j] ) mat[i][j] += 1;
      }
    }
  }

  const pair<bool,int> is_mature(){
    for( int i = 0; i != this->size(); ++i){
      if( this->at(i).size() != 0 && !this->at(i).is_leaf){
	return make_pair(false,i);
      }
    }
    return make_pair(true,-1);
  }

private:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const unsigned int version)
  {
    ar & boost::serialization::base_object< vector<Node> >(*this);
    ar & materials;
  }

};

ostream& operator<<(ostream& out, const Tree& obj){
  out << "-----------------------" << endl;
  for( int i = 0; i!=obj.size(); ++i ){
    out << "------ index=" << i << endl << obj[i];
  }
  out << "**** oob samples ****" << endl;
  foreach ( Sample *s,  obj.materials.second ){
    out << s->name << "(" << s->cls << ") <-> " << obj.classify( *s ) << ":" << (obj.judge( *s )?"o":"x") << endl;
  }
  out << "-----------------------" << endl;
  return out;
}

class Forest{
  
public:
  //#proximity matrix [][]
  multi_array<double,2> prox_mat;
  //#oob oriented proximity matrix [][]
  multi_array<double,2> oob_prox_mat;
  //# mutual oob matirx [][]
  multi_array<double,2> mutual_oob_mat;

  //# number of trees
  int cnt;// = int(arg_vals[3] )
  //# parameter M
  int m;// = int(arg_vals[4] )
  //# list of trees []
  vector<Tree> trees;
  //# attr_len
  int attr_len;
  // # for convinience len( sample_list )
  int sample_len;
  // #output prefix
  string prefix;// = arg_vals[5]

  //#[] takes count for the sample in oob
  vector<int> oob_count;
  //#[] takes count for hit when the sample in oob
  vector<int> oob_hit;// = None
  //#[] takes count for the attribute used
  vector<int> var_count;
  //#[] takes count for difference between ture count and permutated true count
  vector<int> var_diff;
  // [] takes count for attributes relativly important
  vector<int> i_var_count;
  // [] 
  vector<int> i_var_diff;
  //# original
  //# +1 when the attribute is used and the sample is in oob
  multi_array<double,2> coinci_mat;
  //# +1 when the sample judge is missed at the time of the attr is permutated
  //# sample attribute interaction matrix (sai mat)
  multi_array<double,2> sai_mat;

  Forest(){}

  Forest(int _sample_len, int _attr_len, int _cnt, int _m, string _prefix )
    :sample_len(_sample_len),attr_len(_attr_len),cnt(_cnt),m(_m),prefix(_prefix){

    // # init for other list
    for( int i=0;i!=sample_len;i++) oob_count.push_back(0);
    // #[] takes count for hit when the sample in oob
    for( int i=0;i!=sample_len;i++) oob_hit.push_back(0);
    // #[] takes count for the attribute used
    for( int i=0;i!=attr_len;i++) var_count.push_back(0);
    //#[] takes count for difference between ture count and permutated true count
    for( int i=0;i!=attr_len;i++) var_diff.push_back(0);
    //
    for( int i=0;i!=attr_len;i++) i_var_count.push_back(0);
    //
    for( int i=0;i!=attr_len;i++) i_var_diff.push_back(0);

    prox_mat.resize( extents[sample_len][sample_len] );
    //#oob oriented proximity matrix [][]
    oob_prox_mat.resize( extents[sample_len][sample_len] );
    //# mutual oob matirx [][]
    mutual_oob_mat.resize( extents[sample_len][sample_len] );

    //multi_array<double,2> coinci_mat.resize( extents[attr_len][sample_len] );
    coinci_mat.resize( extents[attr_len][sample_len] );
    sai_mat.resize( extents[attr_len][sample_len] );
  }
  
  /*
   * if t.size () == 1 it's a invalid tree!
   */
  const Tree construct_a_tree( pvsvs _data ){
    Node n;
    n.samples = _data.first;
    Tree t(_data);
    t.push_back( n );

    //# return None if first node is homo!!!!
    pair<bool,int> _temp = t.is_mature();
    if(_temp.first) return t;
    queue<int> rem_work;
    rem_work.push( _temp.second );
    while(!rem_work.empty()){
      int index = rem_work.front();
      rem_work.pop();
      // #pick up variables
      int best_index(0);
      double best_delta_i(-1.0);
      double _threshold(-1.0);
      vector<int> attr_len_vec(range_vector(attr_len));
      vector<int> rand_var(m);
      random_sample_n( attr_len_vec.begin() , attr_len_vec.end() , rand_var.begin() , m , rng );
      random_shuffle( rand_var.begin() , rand_var.end() );
      foreach( int i , rand_var ){
	pair<double,double> best_split = get_best_split( get_attr_cls_map( t[index].samples , i ) , gini );
	double _th = best_split.first;
	double _di = best_split.second;
	if( _di > best_delta_i){
	  best_index = i;
	  best_delta_i = _di;
	  _threshold = _th;
	}
      }
      t[index].div_attr = best_index;
      t[index].div_value = _threshold;
      t[index].right_child = t.size()+ 1;
      t[index].left_child = t.size();
      Node right = Node();
      right.parent_index = index;
      Node left = Node();
      left.parent_index = index;
      vector<Sample*> r;
      vector<Sample*> l;
      
      foreach( Sample* v, t[index].samples){
	if( v->data[t[index].div_attr] < t[index].div_value) l.push_back( v );
	else r.push_back( v );
      }
      left.set_data( l );
      right.set_data( r );
      t.push_back( left );
      t.push_back( right );
      if( !right.is_leaf ) rem_work.push( t[index].right_child );
      if( !left.is_leaf ) rem_work.push( t[index].left_child );
    }
    return t;
  }
  
  void grow( SampleSet& ss ){
    int _cnt(0);
    while(_cnt < cnt){
      #ifdef BALANCED
      pvsvs vec = ss.get_random_list_balanced( );
      #else
      pvsvs vec = ss.get_random_list( );
      #endif
      //oob mode --------------
      if( vec.second.size() == 0 ){
	cout << "OOB vector size is 0!" << endl;
	continue;
      }
      Tree t = construct_a_tree( vec );
      if( t.size() == 1 ){
	cout << "Tree's size is 1!" << endl;
	continue;
      }
      // oob based accuracy
      vector<Sample*>& _oob = t.materials.second;
      double suc_cnt = 0.0;
      foreach( Sample *s, _oob){
	if( t.judge( *s ) ){
	  suc_cnt += 1.0;
	}
      }
      double oob_based_accuracy = 100.0 * (suc_cnt / _oob.size());
#ifndef NDEBUG
      cout << _oob.size() << "\t" << suc_cnt << "\t"  << oob_based_accuracy << endl;
#endif
      if( oob_based_accuracy > 60.0 ){
	trees.push_back( t );
	_cnt += 1;
      }
    }
  }
  
  
  vector< map<int,int> > test_samples( SampleSet& ss, SampleSet& test_set ){
    // sample order of test_set and test_results must be identified!!
    vector< map<int,int> > test_results;
    int _cls;
    for( int i=0;i!=test_set.sample_len;i++){
      map<int,int> _m;
      foreach( _cls, test_set.cls_ids ) _m[_cls] = 0;
      test_results.push_back(_m);
    }
    foreach( Tree t , trees ){
      for( int i=0;i!=test_set.sample_len;i++){
	_cls = t[t.classify(test_set[i])].get_cls();
	test_results[i][_cls] += 1;
      }
    }
    return test_results;
  }
  
  void oob_error_estimate( SampleSet& ss ){
    foreach( Tree t, trees ){
      int _true_cnt(0);
      vector<Sample*>& _oob = t.materials.second;
      map<int,bool> __oob_hit;//# index : oob cls test success ( for sai mat)
      map<int,int> _oob_terminal_index;// oob index : termina index in which this oob fall

      foreach( Sample *s, _oob){
	int i = s->index;
	oob_count[i] += 1;
	_oob_terminal_index[i] = t.classify( *s );
	if( t.judge( *s ) ){
	  oob_hit[i] += 1;
	  _true_cnt += 1;
	  __oob_hit[i] = true;
	}
      }
      vector<int> dattrs( t.get_div_attr_list() );
      map<int,int> dattrs_size( t.get_div_attr_and_size_list() );// divide attribute index and the size of that node

      //# update coinici matrix
      foreach( int i, dattrs){
	foreach( Sample *s, _oob){
	  int j = s->index;
	  int ss = coinci_mat.size();
	  coinci_mat[i][s->index] += 1.0;
	}
      }

      // # variable importance
      foreach( int v, dattrs){
	vector<double> perm_data( ss.get_random_perm_data( _oob , v ) );

	#ifndef NDEBUG
	cout << "perm_index:" << v << "\t";
	foreach( double d , perm_data ){
	  cout << d << "\t";
	}
	cout << endl;
	#endif

	int _permu_true_cnt(0);
	for(int i=0;i!=_oob.size();++i){

	  #ifndef NDEBUG
	  cout << _oob[i]->name << ":" << t.judge( *_oob[i], v, perm_data[i] ) << endl;
	  #endif

	  if( t.judge( *_oob[i], v, perm_data[i] ) ){
	    _permu_true_cnt += 1;
	  }else{
	    if( __oob_hit.find( _oob[i]->index ) != __oob_hit.end() && __oob_hit[ _oob[i]->index ] ){// # True when pre permutation
	      sai_mat[v][_oob[i]->index] += 1.0;
	    }
	  }
	}
	var_diff[ v ] += ( _true_cnt - _permu_true_cnt );
	var_count[ v ] += 1;
	if( dattrs_size[v] < 0.8*sample_len && dattrs_size[v] > 0.2*sample_len ) {
	  i_var_diff[ v ] += ( _true_cnt - _permu_true_cnt );
	  i_var_count[ v ] += 1;
	}
      }
      //    # whole prox mat
      t.update_prox_mat( prox_mat , ss , _oob );
      //    # oob prox mat
      foreach( Sample *si, _oob ){
	foreach( Sample *sj, _oob){
	  int i = si->index;
	  int j = sj->index;
	  mutual_oob_mat[i][j] += 1.0;
	  if( _oob_terminal_index[i] == _oob_terminal_index[j] ){
	    oob_prox_mat[i][j] += 1.0;
	  }
	}
      }
    }
  }

  void post_process( ){
    //post-processing ------------------------------------------------------
    //for whole prox_mat
    for( int i=0; i!=sample_len; ++i ){
      for( int j=0; j!=sample_len; ++j ){
	prox_mat[i][j] /= cnt;
      }
    }
    //for oob proximity matrix
    for( int i=0; i!=sample_len; ++i ){
      for( int j=0; j!=sample_len; ++j ){
	oob_prox_mat[i][j] /= mutual_oob_mat[i][j];
      }
    }
    //for sai_mat
    for( int i=0; i!=attr_len; ++i ){
      for( int j=0; j!=sample_len; ++j){
	if( coinci_mat[i][j] == 0 ){
	  sai_mat[i][j] = -1;
	}else {
	  sai_mat[i][j] /= coinci_mat[i][j];
	}
      }
    }
  }

  void export_to_file( string _prefix, SampleSet& ss ){
    ofstream outfile( (_prefix + "_data.txt").c_str() , fstream::out);
    //sample name
    outfile << "name";
    foreach( Sample s , ss){
      outfile << "\t" << s.name;
    }
    outfile << endl;
    //cls
    outfile << "cls";
    foreach( Sample s , ss){
      outfile << "\t" << s.cls;
    }
    outfile << endl;
    //oob_hit
    outfile << "oob_hit";
    foreach( int v , oob_hit ){
      outfile << "\t" << v;
    }
    outfile << endl;
    //oob_count
    outfile << "oob_count";
    foreach( int v , oob_count ){
      outfile << "\t" << v;
    }
    outfile << endl;
    //prox_mat
    outfile << "prox_mat" << endl;
    for( int i = 0; i != sample_len; ++i ){
      for( int j = 0; j != sample_len; ++j ) {
	if( j != 0 ) outfile << "\t";
	outfile << prox_mat[i][j];
      }
      outfile << endl;
    }
    //oob_prox_mat
    outfile << "oob_prox_mat" << endl;
    for( int i = 0; i != sample_len; ++i ){
      for( int j = 0; j != sample_len; ++j ) {
	if( j != 0 ) outfile << "\t";
	outfile << oob_prox_mat[i][j];
      }
      outfile << endl;
    }
    //sai_mat
    outfile << "sai_mat" << endl;
    for( int i = 0; i != attr_len; ++i ){
      for( int j = 0; j != sample_len; ++j ) {
	if( j != 0 ) outfile << "\t";
	outfile << sai_mat[i][j];
      }
      outfile << endl;
    }
    outfile.close();

    outfile.open( (_prefix + "_attr.txt").c_str() , fstream::out);
    //sample name
    outfile << "attr_name\tdiff\tcount\tratio\ti_diff\ti_count\ti_ratio" << endl;
    for( int i = 0;i < ss.attr_list.size(); ++i){
      outfile << ss.attr_list[i] << '\t'
	      << var_diff[i] << '\t'
	      << var_count[i] << '\t'
	      << double(var_diff[i]) / var_count[i] << '\t'
	      << i_var_diff[i] << '\t'
	      << i_var_count[i] << '\t'
	      << double(i_var_diff[i]) / i_var_count[i]
	      << endl;
    }
    outfile.close();
  }


private:
  
  vector< pair<double,int> > get_attr_cls_map( vector<Sample*>& data_set, int attr_index){
    vector< pair<double,int> > result;
    foreach( Sample* s, data_set){
      result.push_back( make_pair(s->data[attr_index],s->cls) );
    }
    return result;
  }


  vector<double> get_split_list( const vector< pair<double,int> >& attr_cls_map ){
    set<double> _temp;
    //foreach( pair<double,int> v , attr_cls_map ){
    for( vector< pair<double,int> >::const_iterator itr = attr_cls_map.begin(); 
	 itr != attr_cls_map.end(); ++itr){
      _temp.insert( itr->first );
    }
    vector<double> l(_temp.begin(),_temp.end());
    sort( l.begin() , l.end() );
    vector<double> result;
    for( int i = 0;i!= l.size()-1;++i){
      result.push_back((l[i]+l[i+1])/2.0);
    }
    return result;
  }

  pair<double,double> get_best_split( const vector< pair<double,int> >& attr_cls_map, eval_func func ){
    double threshold_val(-1.0);
    double delta_i(-1.0);
    double length( attr_cls_map.size() );
    vector<double> sl = get_split_list( attr_cls_map );
    foreach( double v, sl){
      vector<double> cnt(4);
      for( vector< pair<double,int> >::const_iterator itr = attr_cls_map.begin(); 
	   itr != attr_cls_map.end(); ++itr){
	if( itr->first < v ){
	  if( itr->second == -1 ) cnt[0] += 1.0;
	  else cnt[1] += 1.0;
	}else {
	  if( itr->second == -1 ) cnt[2] += 1.0;
	  else cnt[3] += 1.0;
	}
      }
      double temp_delta_i = func( (cnt[0] + cnt[2])/length );
      temp_delta_i -= ((cnt[0]+cnt[1])/length) * func(cnt[0]/(cnt[0]+cnt[1]));
      temp_delta_i -= ((cnt[2]+cnt[3])/length) * func(cnt[2]/(cnt[2]+cnt[3]));
      if( temp_delta_i > delta_i){
	delta_i = temp_delta_i;
	threshold_val = v;
      }
    }
    return make_pair(threshold_val,delta_i);
  }

  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const unsigned int version)
  {
    ar & cnt;
    ar & m;
    ar & trees;
    ar & attr_len;
    ar & sample_len;
    ar & prefix;
    ar & oob_count;
    ar & oob_hit;
    ar & var_count;
    ar & var_diff;
    ar & i_var_count;
    ar & i_var_diff;
  }

};

#endif
