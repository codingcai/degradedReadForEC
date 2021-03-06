/**
  * @file storages/swift.h
  * @author Henry Chen (chchen@cse.cuhk.edu.hk)
  * @brief Declares the SwiftStorage class.
  * **/

/* ===================================================================
Copyright (c) 2013, Henry C. H. Chen
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

  - Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

  - Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.

  - Neither the name of the Chinese University of Hong Kong nor the
    names of its contributors may be used to endorse or promote
    products derived from this software without specific prior written
    permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
=================================================================== */


#ifndef NCCLOUD_STORAGES_SWIFT_H
#define NCCLOUD_STORAGES_SWIFT_H

#include <map>
#include <string>
#include <vector>

#include "../storage.h"


/** Storage module class for OpenStack Swift. */
class SwiftStorage: public Storage
{
  std::string container;
  std::string authurl;
  std::string username;
  std::string password;

  /** Partial command line for running the swift CLI.
   *  This is initialized during SwiftStorage::init().
   *  Format:
   *    swift -q -A "authurl" -U "username" -K "password"
   *  "container" will be added in during run_cmd(), along with the required
   *  actions and filenames to operate on. */
  std::string cmd;


  /** Escapes double quotes (") and backslashes (\). */
  void sanitize(std::string &token);


  /** Invokes the swift CLI.
   *  param[in]         action operation we want the swift CLI to carry out
   *  param[in]           args list of filenames we want the swift CLI to act on
   *  param[in]            cmd partial command line for running the swift CLI
   *  param[in] discard_stdout redirect stdout to /dev/null if not 0
   * */
  int run_cmd(std::string &action, std::vector<std::string> &args,
              std::string &cmd, int discard_stdout=0);


  /** Change directory to dir before invoking swift.
   *  This is because swift acts on the current directory.
   *  This change will not affect the working directory of the rest of the
   *  program since it is executed within a separate shell. */
  int run_cmd(std::string &action, std::vector<std::string> &args,
              std::string &cmd, std::string dir, int discard_stdout=0);

public:
  SwiftStorage();
  int init(std::map<std::string,std::string> &storage_param);

  int store_chunk(std::string &srcdir, std::string &filename, int chunk_index);
  int store_chunks(std::string &srcdir, std::string &filename,
                   std::vector<int> &chunk_indices);
  int store_metadata(std::string &srcdir, std::string &filename);
  int store_metadata_and_chunks(std::string &srcdir, std::string &filename,
                                std::vector<int> &chunk_indices);

  int get_chunk(std::string &dstdir, std::string &filename, int chunk_index);
  int get_chunks(std::string &dstdir, std::string &filename,
                 std::vector<int> &chunk_indices);
  int get_metadata(std::string &dstdir, std::string &filename);
  int get_metadata_and_chunks(std::string &dstdir, std::string &filename,
                              std::vector<int> &chunk_indices);

  int delete_chunk(std::string &filename, int chunk_index);
  int delete_chunks(std::string &filename, std::vector<int> &chunk_indices);
  int delete_metadata(std::string &filename);
  int delete_metadata_and_chunks(std::string &filename,
                                 std::vector<int> &chunk_indices);

  int list_files(void);
  int check_health(void);
};

#endif  /* NCCLOUD_STORAGES_SWIFT_H */

