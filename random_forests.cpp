/*
Copyright 2010 SHINGO TSUJI. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are
permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this list of
      conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice, this list
      of conditions and the following disclaimer in the documentation and/or other materials
      provided with the distribution.

THIS SOFTWARE IS PROVIDED BY SHINGO TSUJI ``AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SHINGO TSUJI OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those of the
authors and should not be interpreted as representing official policies, either expressed
or implied, of SHINGO TSUJI.
*/

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
