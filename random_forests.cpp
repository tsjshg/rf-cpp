#include <cmath>
#include <boost/program_options.hpp>
#include "random_forests.hpp"

namespace po = boost::program_options;

  /*
in random_forests.hpp
function grow
      //pvsvs vec = ss.get_random_list_balanced( );
      //pvsvs vec = ss.get_random_list_weighted( );
      pvsvs vec = ss.get_random_list( );
output binary will be changed.
   */
int main (int argc, char **argv) {
  
  po::options_description opt("options");
  opt.add_options()
    ("help,h","display this help")
    ("data,d",po::value<string>(),"data file")
    ("cls,c",po::value<string>(),"class file")
    ("trees,t",po::value<int>()->default_value(100000), "number of trees (default=100,000)")
    ("M,m", po::value<int>(), "m (default=sqrt(number of attributes)")
    ("prefix,p", po::value<string>(), "prefix of output files")
    ("attr_list,a", po::value<string>(), "list of attributes (file with no header)")
    ("obj_file,o", po::value<string>(), "archiving object file name");
  
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, opt), vm);
  po::notify(vm);

  if( vm.count("help") || !vm.count("data") 
      || !vm.count("cls") 
      || !vm.count("prefix") ){
    cout << opt << endl; // ヘルプ表示
    return 0;
  }

  string data_file;
  string cls_file;
  string attr_list("");
  data_file = vm["data"].as<string>();
  cls_file  = vm["cls"].as<string>();
  if( vm.count("attr_list") ){
    attr_list = vm["attr_list"].as<string>();
  }

  SampleSet ss( data_file , cls_file , attr_list );
  int m;
  if( !vm.count("m") ){
    m = (int)ceil(sqrt((double)ss.attr_len));
  }else {
    m = vm["M"].as<int>();
  }
  string _prefix = vm["prefix"].as<string>();
  int trees = vm["trees"].as<int>();
    
  Forest f( ss.sample_len, ss.attr_len, trees, m, _prefix );

  f.grow( ss );
  f.oob_error_estimate( ss );
  f.post_process();
  f.export_to_file( _prefix, ss );
  
  ofstream obj_outfile( vm["obj_file"].as<string>().c_str() , fstream::out);
  binary_oarchive oa(obj_outfile);
  oa << (const SampleSet&) ss;
  oa << (const Forest&) f;

  #ifndef NDEBUG
  foreach( Tree t, f.trees ){
    cout << t;
  }  

  cout << endl;
  cout << "oob_count" << endl;
  foreach( int i , f.oob_count ){
    cout << i << "\t";
  }
  cout << endl;

  cout << "oob_hit" << endl;
  foreach( int i , f.oob_hit ){
    cout << i << "\t";
  }
  cout << endl;

  cout << "var_count" << endl;
  foreach( int i , f.var_count ){
    cout << i << "\t";
  }
  cout << endl;

  cout << "var_diff" << endl;
  foreach( int i , f.var_diff ){
    cout << i << "\t";
  }
  cout << endl;
  
  cout << "prox_mat" << endl;
  for( int i = 0 ; i < f.sample_len; ++i){
    for( int j = 0 ; j < f.sample_len; ++j){
      cout << f.prox_mat[i][j] << "\t";
    }
    cout << endl;
  }

  cout << "oob_prox_mat" << endl;
  for( int i = 0 ; i < f.sample_len; ++i){
    for( int j = 0 ; j < f.sample_len; ++j){
      cout << f.oob_prox_mat[i][j] << "\t";
    }
    cout << endl;
  }
  
  cout << "mutual_oob_mat" << endl;
  for( int i = 0 ; i < f.sample_len; ++i){
    for( int j = 0 ; j < f.sample_len; ++j){
      cout << f.mutual_oob_mat[i][j] << "\t";
    }
    cout << endl;
  }

  cout << "coinci_mat" << endl;
  for( int i = 0 ; i < f.attr_len; ++i){
    for( int j = 0 ; j < f.sample_len; ++j){
      cout << f.coinci_mat[i][j] << "\t";
    }
    cout << endl;
  }

  cout << "sai_mat" << endl;
  for( int i = 0 ; i < f.attr_len; ++i){
    for( int j = 0 ; j < f.sample_len; ++j){
      cout << f.sai_mat[i][j] << "\t";
    }
    cout << endl;
  }
  #endif

  return 0;

}
