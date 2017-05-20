/**
 * @file fileop.cc
 * @author Henry Chen (chchen@cse.cuhk.edu.hk)
 * @brief Implements the Job and FileOp classes, and also job pipelining.
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

#include <stdlib.h>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <sys/time.h>
#include <map>
#include <utility>
#include <vector>
#include <algorithm>
#include "databaseInterface.h"
#include "fileop.h"

using namespace std;

/*  -------------------------  */
/* | Thread-related routines | */
/*  -------------------------  */
static int num_working_threads = 0;
static queue<Job *> storage_queue;
static queue<Job *> coding_queue;
static condition_variable no_working_threads;
static condition_variable storage_queue_ready;
static condition_variable coding_queue_ready;
static mutex master_mutex;

/** Add a pointer to an object describing a job to the job queue q. */
static void add_job(Job *job, queue<Job *> &q, mutex &m,
		condition_variable &cv) {
	m.lock();
	q.push(job);
	m.unlock();
	cv.notify_all();
}

/** Wait until there is a job available for processing.
 *  @param[out] job pointer to a Job object describing the next job
 *  @return 0 if there are no more job (next job is a NULL pointer). */
static int wait_job(Job* &job, queue<Job *> &q, mutex &m,
		condition_variable &cv) {
	int end = 0;
	unique_lock < mutex > lock(m);

	if (--num_working_threads == 0) {
		no_working_threads.notify_all();
	}
	while (q.empty()) {
		cv.wait(lock);
	}
	num_working_threads++;

	// doesn't pop when next job is NULL, leave it for the other threads to see
	if (!(end = (q.front() == NULL))) {
		job = q.front();
		q.pop();
	}
	m.unlock();
	return !end;
}

/** Run thread indefinitely, wait for jobs to process
 *  and quit when there will be no more jobs */
static void run_thread(queue<Job *> &q, mutex &m, condition_variable &cv) {


	m.lock();
	num_working_threads++;
	m.unlock();

	Job *job = NULL;
	while (wait_job(job, q, m, cv)) {
		//如果队列有任务继续完成
		job->run_job();
		delete job;
	}

}

/*  ---------------------------------  */
/* | ------------------------------- | */
/* ||           Job Class           || */
/* | ------------------------------- | */
/*  ---------------------------------  */

/*  ----------------  */
/* | Public methods | */
/*  ----------------  */
Job::Job(int action, Coding *coding, vector<Storage *> *storages,
		string &tmpdir, string &filename) :
		action(action), coding(coding), storages(storages), tmpdir(tmpdir), filename(
				filename), next_job(NULL) {
}

void Job::follow_job(void) {
	if (next_job != NULL) {
		if (next_job->action < DIVIDER) {
			add_job(next_job, storage_queue, master_mutex, storage_queue_ready);
		} else {
			add_job(next_job, coding_queue, master_mutex, coding_queue_ready);
		}
	}
}

void Job::run_job(void) {
	switch (action) {
	case ULMETACHUNKS:
		//upload_metadata_and_chunks();
		upload_metadata_and_chunks_for_record_time();
		break;
	case ULMETA:
		upload_metadata();
		break;
	case DLCHUNKS:
		//download_chunks();
		download_chunks_for_record_time();
		break;
	case DLMETA:
		download_metadata();
		break;
	case DECODE:
		decode_file();
		break;
	case REPAIR:
		repair_file();
		break;
	default:
		print(stringstream() << "Invalid job received: " << action << endl);
	}
	follow_job();
}

/*  ------------------------------------  */
/* | Private methods (the job routines) | */
/*  ------------------------------------  */

void Job::upload_metadata_and_chunks(void) {
	// upload metadata and chunks on a per-node basis
	for (auto nodeid : node_indices) {
		vector<int> cur_chunk_indices;
		for (auto chunk_index : chunk_indices) {
			if (coding->nodeid(chunk_index) == nodeid) {
				cur_chunk_indices.push_back(chunk_index);
			}
		}
		if ((*storages)[nodeid]->store_metadata_and_chunks(tmpdir, filename,
				cur_chunk_indices) == -1) {
			stringstream s;
			s << "Failed to upload " << tmpdir << "/" << filename;
			for (auto cur_chunk_index : cur_chunk_indices) {
				s << " [" << cur_chunk_index << "]";
			}
			s << " to node " << nodeid << endl;
			print_error(s);
			exit(-1);
		}
	}
}

/*
 * Add by CaiYi
 * 2014/4/10
 * New method to upload chunks with recording the time
 */

void Job::upload_metadata_and_chunks_for_record_time(void) {
	// upload metadata and chunks on a per-node basis
	struct timeval start_file;
	struct timeval end_file;
	gettimeofday(&start_file,NULL);
	for (auto nodeid : node_indices) { // in the encode_file, the elements are pushed back
		struct timeval start;
		struct timeval end;
		gettimeofday(&start,NULL);
		vector<int> cur_chunk_indices;
		for (auto chunk_index : chunk_indices) {
			if (coding->nodeid(chunk_index) == nodeid) {
				cout << "chunk index:  " << chunk_index << endl; //
				cur_chunk_indices.push_back(chunk_index);
			}
		}
        /*
         * 这里暂时不记录写的时间  但是因为要在数据库中增加某个节点的信息 因此记录为0
         */

		//it's included in the for... So there store in the one node
		if ((*storages)[nodeid]->store_metadata_and_chunks(tmpdir, filename,
				cur_chunk_indices) == -1) {
			stringstream s;
			s << "Failed to upload " << tmpdir << "/" << filename;
			for (auto cur_chunk_index : cur_chunk_indices) {
				s << " [" << cur_chunk_index << "]";
			}
			s << " to node " << nodeid << endl;
			print_error(s);
			exit(-1);
		}
		gettimeofday(&end,NULL);

		double write_time  = end.tv_sec-start.tv_sec+(end.tv_usec-start.tv_usec)/1000000.0;;
		std::cout << "the node of "
				<< (*storages)[nodeid]->get_repository_path()
				<< "   .The time is " << write_time << endl; //only used by local;
		//we could get the path : (*storages)[nodeid]->get_repository_path()
		store_write_time(nodeid,  write_time);
		cout << endl;
	}
	gettimeofday(&end_file,NULL);
    //这个是一个文件的写时间
	double timer = end_file.tv_sec-start_file.tv_sec+(end_file.tv_usec-start_file.tv_usec)/1000000.0;
	cout<<"upload on file's  time is   "<<timer<<" s"<<endl;
}
/*
 * @param node_id : the node index:0 1 2...
 * @param write_time: each write responde time
 */

void Job::store_write_time(int node_id, double write_time) {  //写数据时 初始化数据库
	//MySQLInterface *mysql = MySQLInterface::GetInstance();
	write_time = 0 ; //将写时间暂时设置为0
	MySQLInterface *mysql = new MySQLInterface();
	mysql->SetMySQLConInfo("localhost", "root", "cai", "performance", 337);
	if (!mysql->Open()) {
        std::cout << mysql->ErrorNum << " : " << mysql->ErrorInfo << std::endl;
    }

	vector<vector<string>> select_result;
	string select_one_node = "SELECT * FROM nodePerformance WHERE nodeName='"
			+ to_string(node_id) + "'";
	//cout << select_one_node << endl;
	mysql->Select(select_one_node, select_result);
	if (select_result.empty()) {   //如果对应某一个节点的信息为空 那么将这个节点信息写入， accessNumber置为0
		cout << "the node of " << to_string(node_id) << "  is empty" << endl;
		string create_node_info =
				"INSERT INTO  nodePerformance(nodeName,respondeTime,accessNumber) VALUES('"
						+ to_string(node_id) + "'," + to_string(write_time)
						+ "," + to_string(0) + ")";
		mysql->GetInsertID(create_node_info); //insert the nodeName respondeTime to the table
	}
	for (int i = 0; i < select_result.size(); i++) {
		for (int j = 0; j < select_result[0].size(); j++)
			cout << select_result[i][j] << " ";
		cout << endl;
	}
    mysql->Close();
}

#define LAST_RADIO 0.9
#define NOW_RADIO 0.1

void Job::store_read_time(int node_id, double write_time) {  //读时间这里记录  也就是解码操作
    cout<<"the node_id is "<<node_id<<"  and the write_time is "<<write_time<<endl;
    //MySQLInterface *mysql = MySQLInterface::GetInstance();
    MySQLInterface *mysql = new MySQLInterface();
    mysql->SetMySQLConInfo("localhost", "root", "cai", "performance", 337);
    if (!mysql->Open()) {
        std::cout << mysql->ErrorNum << " : " << mysql->ErrorInfo << std::endl;
    }


    //select 某一个节点的信息是否已经存在如果不存在则写入，如果存在，则更新

    vector<vector<string>> select_result; //这里select_result 的size是1
    string select_one_node = "SELECT * FROM nodePerformance WHERE nodeName='"
                             + to_string(node_id) + "'";
    //cout << select_one_node << endl;
    mysql->Select(select_one_node, select_result);
    if (select_result.empty()) {   //如果对应某一个节点的信息为空 那么将这个节点信息写入， accessNumber置为1
        cout << "the node of " << to_string(node_id) << "  is empty" << endl;
        string create_node_info =
                "INSERT INTO  nodePerformance(nodeName,respondeTime,accessNumber) VALUES('"
                + to_string(node_id) + "'," + to_string(write_time)
                + "," + to_string(1) + ")";
        mysql->GetInsertID(create_node_info); //insert the nodeName respondeTime to the table
    } else {


        string string_respondeTime = select_result[0][1]; //select_result[0][1] is respindeTime in the nodePerformance
        string string_accessTime = select_result[0][2]; //select_result[0][2] is accessTime in the nodePerformance
        float respondeTime = atof(string_respondeTime.c_str());
        int accessTime = atof(string_accessTime.c_str());
        accessTime++; //每次写数据时，对应节点的访问次数加1
        respondeTime = respondeTime * LAST_RADIO + write_time * NOW_RADIO; //每次写数据时，对应节点的访问次数加1

        /*
        string update_write_info = "UPDATE nodePerformance SET respondeTime="
                + to_string(respondeTime) + ", accessNumber="
                + to_string(accessTime) + " WHERE nodeName='"
                + to_string(node_id) + "'";
        */

        /**
         * 这里只更新响应时间，访问次数在选择节点时已经修改。
         */
        string update_write_info = "UPDATE nodePerformance SET respondeTime="
                                   + to_string(respondeTime)  + " WHERE nodeName='"
                                   + to_string(node_id) + "'";


        //update the write time and write number
        bool update = mysql->Query(update_write_info);

    }
    cout<<"node index       average time     access time"<<endl;
    for (int i = 0; i < select_result.size(); i++) {
        for (int j = 0; j < select_result[0].size(); j++)
            cout << select_result[i][j] << " ";
        cout << endl;
    }
    mysql->Close();
    delete mysql;
}






void Job::upload_metadata(void) {
	// upload metadata to each node
	for (auto nodeid : node_indices) {
		if ((*storages)[nodeid]->store_metadata(tmpdir, filename) == -1) {
			print_error(
					stringstream() << "Failed to upload metadata of "
							<< filename << " to node " << nodeid << endl);
			exit(-1);
		}
	}
}

void Job::download_chunks(void) {
	// download chunks on a per-node basis
	/*
	 *just for test
	 */
	for (auto i : node_indices) {
		cout << "node indix:  " << i << endl;
		cout << endl;
	}

	for (auto nodeid : node_indices) {
		vector<int> cur_chunk_indices;
		for (auto chunk_index : chunk_indices) {
			if (coding->nodeid(chunk_index) == nodeid) {
				cur_chunk_indices.push_back(chunk_index);
			}
		}
		if ((*storages)[nodeid]->get_chunks(tmpdir, filename, cur_chunk_indices)
				== -1) {
			stringstream s;
			s << "Failed to download " << filename;
			for (auto cur_chunk_index : cur_chunk_indices) {
				s << " [" << cur_chunk_index << "]";
			}
			s << " from node " << nodeid << endl;
			print_error(s);
			exit(-1);
		}
	}
}


void Job::download_chunks_for_record_time(void)
{
	// download chunks on a per-node basis and record the time
	/*
	 *just for test
	 */
	for (auto i : node_indices) {
		cout<<"**********************DOWNLOAD CHUNKS*****************"<<endl;
		cout << "node indix:  " << i << endl;
		cout << endl;
	}
    struct timeval start_file;
    struct timeval end_file;
    //一个文件读文件所花费的时间
    gettimeofday(&start_file,NULL);
	for (auto nodeid : node_indices) {
		struct timeval start_read_in_node;
        struct timeval end_read_in_node;
		gettimeofday(&start_read_in_node,NULL);
		vector<int> cur_chunk_indices;
		for (auto chunk_index : chunk_indices) {
			if (coding->nodeid(chunk_index) == nodeid) {
				cur_chunk_indices.push_back(chunk_index);
			}
		}
        //在get_chunk中有打印目录信息
		if ((*storages)[nodeid]->get_chunks(tmpdir, filename, cur_chunk_indices)
				== -1) {
			stringstream s;
			s << "Failed to download " << filename;
			for (auto cur_chunk_index : cur_chunk_indices) {
				s << " [" << cur_chunk_index << "]";
			}
			s << " from node " << nodeid << endl;
			print_error(s);
			exit(-1);
		}
		gettimeofday(&end_read_in_node,NULL);
		double read_time =  end_read_in_node.tv_sec-start_read_in_node.tv_sec+
                (end_read_in_node.tv_usec-start_read_in_node.tv_usec)/1000000.0;
		store_read_time(nodeid,read_time);
	}
     gettimeofday(&end_file,NULL);
     double read_one_file_time = end_file.tv_sec-start_file.tv_sec+(end_file.tv_usec-start_file.tv_usec)/1000000.0;
     cout<<"read one file time is  "<<read_one_file_time<<endl;

}


void Job::download_metadata(void) {
	// download metadata from the first node
	if ((*storages)[node_indices[0]]->get_metadata(tmpdir, filename) == -1) {
		print_error(
				stringstream() << "Failed to download metadata of " << filename
						<< " from node " << node_indices[0] << endl);
		exit(-1);
	}
}

void Job::decode_file(void) {
	string dst = tmpdir + '/' + filename;
	if (coding->decode_file(dst, tmpdir, filename, chunk_indices) == -1) {
		print_error(stringstream() << "Failed to decode: " << filename << endl);
		exit(-1);
	}
}

void Job::repair_file(void) {
	if (coding->repair_file(tmpdir, tmpdir, filename) == -1) {
		print_error(
				stringstream() << "Failed to repair"
						<< " (check that you have invoked repair_file_preprocess()"
						<< " of the corresponding coding scheme): " << filename
						<< endl);
		exit(-1);
	}
}

/*  ------------------------------------  */
/* | ---------------------------------- | */
/* ||           FileOp Class           || */
/* | ---------------------------------- | */
/*  ------------------------------------  */

/*  ----------------  */
/* | Public methods | */
/*  ----------------  */
#define ACCESS_VARIANCE_MAX 20
#define RESPONDE_RADIO 0.95
FileOp *FileOp::instance(void) {
    /*
     * 这里的instance只调用一次因此可以这样使用
     * 但是注意：绝不用返回一个指针或引用只想一个local static对象而有可能同时需要多个这样的对象。
     */
	static FileOp _instance;
	return &_instance;
}

void FileOp::wait(void) {
	// Wait until no one is working, which means there should be no more jobs
	unique_lock < mutex > lock(master_mutex);
	while (num_working_threads != 0 || !storage_queue.empty()
			|| !coding_queue.empty()) {
		no_working_threads.wait(lock);
	}
	master_mutex.unlock();

	// NULL pointers indicate to waiting threads there will be no more jobs
	add_job(NULL, storage_queue, master_mutex, storage_queue_ready);
	add_job(NULL, coding_queue, master_mutex, coding_queue_ready);
	for (auto &t : workers) {
		t.join();
	}
}

void FileOp::encode_file(string &path, Coding *coding,
		vector<Storage *> &storages, string &tmpdir) {
	print(stringstream() << "Encoding: " << path << endl);

	// encode
	int sep = path.length() - 1;
	for (; sep >= 0 && path[sep] != '/'; --sep)
		;
	string srcdir;
	if (sep >= 0) {
		srcdir.assign(path, 0, sep);
	} else {		//if the dir is current dir, the src is .
		srcdir.assign(".");  //
	}

	string filename(path, sep + 1);
	/*
	 * Add by CaiYi
	 */
	cout << "srcdir is " << srcdir << endl;
	cout << "tmpdir is " << tmpdir << endl;
	cout << "filename is " << filename << endl;
	/*
	 * Add ends.
	 */
	//encode_file will encode the file to tmp dir.
	if (coding->encode_file(tmpdir, srcdir, filename) == -1) {
		print_error(
				stringstream() << "Failed to encode: " << srcdir << "/"
						<< filename << endl);
		exit(-1);
	}

	// enqueue job: store_metadata_and_chunks()  to every dir(node0 node1 ..)
	Job *job = new Job(Job::ULMETACHUNKS, coding, &storages, tmpdir, filename);
	for (int i = 0, j = 0; i < coding->getn(); ++i) {
		job->node_indices.push_back(i);
		for (int jj = 0; jj < coding->chunks_per_node(); ++jj, ++j) {
			job->chunk_indices.push_back(j);
		}
	}
	add_job(job, storage_queue, master_mutex, storage_queue_ready);
}

void FileOp::decode_file(string &filename, Coding *coding,
		vector<Storage *> &storages, string &tmpdir) {
	print(stringstream() << "Decoding: " << filename << endl);

	// look for healthy nodes
	int n = coding->getn();
	vector<int> healthy_nodes;
	for (int i = 0; i < n; ++i) {
		if (storages[i]->check_health() == 0) {
			healthy_nodes.push_back(i);
		} else {
			print_error(
					stringstream() << "WARNING: node " << i << " may be down."
							<< endl);
		}
	}

	unsigned int k = (unsigned int) coding->getk();
	if (healthy_nodes.size() < k) {
		print_error(stringstream() << "Insufficient healthy nodes." << endl);
		exit(-1);
	}

	// download chunks from the first k healthy node, and save their chunk indices
	vector<int> chunk_indices;
	for (unsigned int i = 0; i < k; ++i) {
		vector<int> cur_chunk_indices;
		coding->chunks_on_node(healthy_nodes[i], cur_chunk_indices);
		chunk_indices.insert(chunk_indices.end(), cur_chunk_indices.begin(),
				cur_chunk_indices.end());
	}

	// create job 1: download_chunks()
	Job *job1 = new Job(Job::DLCHUNKS, coding, &storages, tmpdir, filename);
	job1->chunk_indices = chunk_indices;
	for (unsigned int i = 0; i < k; ++i) {
		job1->node_indices.push_back(healthy_nodes[i]);
	}

	// create job 2: download_metadata()
	Job *job2 = new Job(Job::DLMETA, coding, &storages, tmpdir, filename);
	job2->node_indices.push_back(healthy_nodes[0]);

	// create job 3: decode_file()
	Job *job3 = new Job(Job::DECODE, coding, &storages, tmpdir, filename);
	job3->chunk_indices = chunk_indices;

	// chain the jobs and enqueue job 1 [download_chunks()]
	job1->next_job = job2;
	job2->next_job = job3;
	add_job(job1, storage_queue, master_mutex, storage_queue_ready);
}

/*
 * Add by CaiYi
 * 2017/4/12
 * File:: void get_sorted_node(vector<vector<string>> node_responde_time);
 */
/*
void FileOp::get_sorted_node(vector<pair<string,double>>& node_responde_time)
{
	//MySQLInterface *mysql = MySQLInterface::GetInstance();
	MySQLInterface *mysql = new MySQLInterface();
	mysql->SetMySQLConInfo("localhost", "root", "cai", "performance", 337);
	if (!mysql->Open()) {
		std::cout << mysql->ErrorNum << " : " << mysql->ErrorInfo << std::endl;
	}
		std::vector<std::vector<std::string>> data;
		std::string sqlstr =
				"SELECT `nodeName`,`respondeTime`,`accessNumber` FROM `performance`.`nodePerformance`";
	    mysql->Select(sqlstr, data);
    mysql->Close();
	    auto cmp =[](std::pair<string,double> const&a,std::pair<string,double> const&b)
			{
	    		return a.second!=b.second? a.second<b.second:a.first<b.first;
			};

		for(int i=0;i<data.size();i++)
		{
			node_responde_time.push_back(make_pair(data[i][0],atof(data[i][1].c_str())));
		}
		sort(node_responde_time.begin(),node_responde_time.end(),cmp);
}

*/



void FileOp::updateAccessTime(const vector<string> nodeAndAccessTime)
{
    MySQLInterface *mysql = new MySQLInterface();
    mysql->SetMySQLConInfo("localhost", "root", "cai", "performance", 337);
    if (!mysql->Open()) {
        std::cout << mysql->ErrorNum << " : " << mysql->ErrorInfo << std::endl;
    }

    for(int i = 0 ;i<nodeAndAccessTime.size();i++)
    {
        string node_id = nodeAndAccessTime[i];
        vector<vector<string>> select_result; //这里select_result 的size是1
        string select_one_node = "SELECT * FROM nodePerformance WHERE nodeName='"
                                 + node_id + "'";
        //cout << select_one_node << endl;
        mysql->Select(select_one_node, select_result);
        if (select_result.empty()) {   //如果对应某一个节点的信息为空 那么将这个节点信息写入， accessNumber置为1
            cout << "the node of " << node_id << "  is empty" << endl;
            string create_node_info =
                    "INSERT INTO  nodePerformance(nodeName,respondeTime,accessNumber) VALUES('"
                    + node_id + "'," + to_string(0)
                    + "," + to_string(1) + ")";
            mysql->GetInsertID(create_node_info); //insert the nodeName respondeTime to the table
        } else {



            string string_accessTime = select_result[0][2]; //select_result[0][2] is accessTime in the nodePerformance

            int accessTime = atoi(string_accessTime.c_str());
            accessTime++; //每次写数据时，对应节点的访问次数加1


            string update_write_info = "UPDATE nodePerformance SET accessNumber="
                                       + to_string(accessTime) + " WHERE nodeName='"
                                       + node_id + "'";

            //update the write time and write number
            bool update = mysql->Query(update_write_info);
            //cout << "update is  " << update << endl;
        }
    }
    mysql->Close();
    delete mysql;

}



/**
 * 为没有响应节点请求的节点更新（降低）响应时间
 * @param reduceRespondTime 需要更新的响应时间 pair中first为节点名称 second为需要更改的时间
 */
void FileOp::updateRespondTime(const vector<pair<string,double>> reduceRespondTime)
{
    MySQLInterface *mysql = new MySQLInterface();
    mysql->SetMySQLConInfo("localhost", "root", "cai", "performance", 337);
    if (!mysql->Open()) {
        std::cout << mysql->ErrorNum << " : " << mysql->ErrorInfo << std::endl;
    }
    // 更新数据

    string tableName = "nodePerformance";
    string updateColumn = "respondeTime";
    string goalRaw = "nodeName";
    char* sqlstr_c="";
    //"SELECT `nodeName`,`respondeTime`,`accessNumber` FROM `performance`.`nodePerformance`";
    for(int i =0;i<reduceRespondTime.size();i++)
    {
        string rawValue = reduceRespondTime[i].first; //这里是id号
        double updateValue = reduceRespondTime[i].second;//这里是修改后的响应时间
        string updateString = "UPDATE `"+tableName+"` SET `"+updateColumn+"`="+to_string(updateValue)+" WHERE `"+
                              goalRaw+"`='"+rawValue+"'";

        cout<<updateString<<endl;
        mysql->Update(updateString);
    }
    mysql->Close();
    delete mysql;
}


/**
 * 应该注意这个sqlData首先要是当前处于良好状态的节点信息，不能包含已经坏了的节点信息
 *
 * @param choiceNode  这是前k个节点 用这k个节点去响应请求
 * @param sqlData   这是才数据库中读入的全部数据，包含了每个节点的respondeTime 和 accessTime
 */
void FileOp::reduceRespondTime(const vector<string>& choiceNode, const vector<vector<string>>& sqlData)
{ //这里传过来的choice node应该是已经选择过的k个节点的信息 而不是n个

    //这里是将响应时间降低20%
    double REDUCE_RADIO =  0.8;
    vector<pair<string,double>> reduce_respond_nodes;
    //找出不需要用作响应请求的n-k个节点。
    for(int i = 0;i<sqlData.size();i++)
    {
        if(find(choiceNode.begin(),choiceNode.end(),sqlData[i][0])==choiceNode.end())
            reduce_respond_nodes.push_back(make_pair(sqlData[i][0],atof(sqlData[i][1].c_str())*REDUCE_RADIO));
    }
    //更新不响应请求的节点的响应时间
    updateRespondTime(reduce_respond_nodes);
}

/**
 *
 * @param sqlData 从数据库中读入的信息，同样应该注意数据库读入的列应该保证它节点状态是良好的
 * @return 返回排序好的n个节点
 */
vector<string> FileOp::choiceNode(const vector<vector<string>>& sqlData,Coding* coding)
{

    vector<string> choiceNodes;


    vector<pair<string,int>> nodeAccessTime;
    vector<pair<string,double>> nodeRespondeTime;

    vector<string> allNodes;
    vector<int> accessTimes;
    vector<double> respondeTimes;


    for(int i=0;i<sqlData.size();i++)
    {
        if(3<=sqlData[i].size())
        {
            string node = sqlData[i][0];
            double respondeTime = atof(sqlData[i][1].c_str());
            int accessTime = atoi(sqlData[i][2].c_str());

            nodeRespondeTime.push_back(pair<string,double>(node,respondeTime));
            nodeAccessTime.push_back(pair<string,int>(node,accessTime));

            accessTimes.push_back(accessTime);
            respondeTimes.push_back(respondeTime);
            allNodes.push_back(node);
        }
    }
    //这里计算访问次数的方差，避免某一个节点访问次数过大。
    double accessTimeVarience = getVariance(accessTimes);
    if(accessTimeVarience<ACCESS_VARIANCE_MAX)
    { //如果方差小于某一阈值 则采用响应时间判断
        getNodesByRespondeTime(choiceNodes,nodeRespondeTime);
    }
    else //否则采用访问次数
        getNodesByAccessTime(choiceNodes,nodeAccessTime);

	cout<<" the access Varience is  "<<accessTimeVarience<<endl;

    int k = coding->getk();
    vector<string> choiceK_nodes;
    for(int i=0;i<k;i++)
        choiceK_nodes.push_back(choiceNodes[i]);

    //没有选中的节点，降低其响应时间
    reduceRespondTime(choiceK_nodes,sqlData);
    //对于选中的节点，对对应节点的访问次数加一
    updateAccessTime(choiceK_nodes);
    return choiceNodes;

}


/**
 *
 * @param choiceNodes 最终排序好的节点信息
 * @param nodeAccessTime 每一个节点的访问时间
 */
void FileOp::getNodesByAccessTime(vector<string>& choiceNodes,vector<pair<string,int>> nodeAccessTime)
{
    /**
     * 这里按照升序排序 也就是访问次数少的将响应请求
     */
    auto cmp =[](std::pair<string,double> const&a,std::pair<string,int> const&b)
    {
        return a.second!=b.second? a.second<b.second:a.first<b.first;
    };

    sort(nodeAccessTime.begin(),nodeAccessTime.end(),cmp);
    for(int i=0;i<nodeAccessTime.size();i++)
    {
        choiceNodes.push_back(nodeAccessTime[i].first);
    }
}


/**
 *
 * @param choiceNodes  排序好的节点
 * @param nodeRespondeTime 节点的响应时间
 */
void FileOp::getNodesByRespondeTime(vector<string>& choiceNodes,vector<pair<string,double>> nodeRespondeTime)
{
    /**
    * 这里按照升序排序 响应时间少的将响应请求
    */
    auto cmp =[](std::pair<string,double> const&a,std::pair<string,double> const&b)
    {
        return a.second!=b.second? a.second<b.second:a.first<b.first;
    };

    sort(nodeRespondeTime.begin(),nodeRespondeTime.end(),cmp);
    for(int i=0;i<nodeRespondeTime.size();i++)
    {
        choiceNodes.push_back(nodeRespondeTime[i].first);
    }
}




/**
 *
 * @param 输入一组数据
 * @return  返回方差
 */
double FileOp::getVariance(vector<int> numbers)
{//计算方差
    int numbersSum = std::accumulate(numbers.begin(),numbers.end(),0);
    double averageNumber = numbersSum/numbers.size();
    double accum  = 0.0;
    std::for_each (std::begin(numbers), std::end(numbers), [&](const double d) {
        accum  += (d-averageNumber)*(d-averageNumber);
    });

    double variance = accum/(numbers.size()-1);//方差
    double stdev = sqrt(variance); //标准差
    cout<<numbersSum<<"   "<<variance<<"   " <<stdev<< endl;
    return variance;
}



/**
 * 获取数据
 * @return  返回从数据库中得到的数据
 */
vector<vector<string>> FileOp::getData()
{
    MySQLInterface *mysql = new MySQLInterface();
    mysql->SetMySQLConInfo("localhost", "root", "cai", "performance", 337);
    if (!mysql->Open()) {
        std::cout << mysql->ErrorNum << " : " << mysql->ErrorInfo << std::endl;
    }
    // 读取数据

    std::vector<std::vector<std::string> > data;

    std::string sqlstr =
            "SELECT `nodeName`,`respondeTime`,`accessNumber` FROM `performance`.`nodePerformance`";

    mysql->Select(sqlstr, data);
    mysql->Close();
    delete mysql;

    // 显示数据
    /*
     for (unsigned int i = 0; i < data.size(); ++i) {

         for (unsigned int j = 0; j < data[0].size(); ++j) {
             cout << data[i][j] << "\t\t";
         }
         cout << endl;
     }
     */
    return data;
}


/**
 *
 * @return  返回排序好的节点名称
 */
vector<string> FileOp::get_sorted_node(Coding* coding)
{
    vector<vector<string>> data = getData();
    vector<string> node_responde_time = choiceNode(data,coding);
    for(int i=0;i<node_responde_time.size();i++)
        cout<<node_responde_time[i]<<"   ";
    return node_responde_time;
}







/*
 * Add by CaiYi
 * 2017/4/12
 *FileOp::decode_file_for_degraded_read
 */
void FileOp::decode_file_for_degraded_read(string &filename, Coding *coding,
		vector<Storage *> &storages, string &tmpdir) {
	print(stringstream() << "Decoding: " << filename << endl);

    //get the node_responde_time  and sort these;
    vector<string>node_responde_time;
    //这里获取到排序好的节点
    node_responde_time=get_sorted_node(coding);

	vector<string>::iterator i = node_responde_time.begin();
	while(i!=node_responde_time.end())
	{
		cout<<*i<<endl;
		i++;
	}


	vector<int> healthy_nodes;
	/*
	 * 当node_responde_time中的个数大于等于k时，
	 * 则将node_responde_time的first元素（节点index 0  1 2等）取出来
	 * 然后如果check_health()是好的节点，则放入到health_nodes这个vector中
	 */
	if(coding->getk()<=node_responde_time.size())
	{

		int n = coding->getn();
		for(int i=0;i<node_responde_time.size();i++)
		{
			string string_nodeIndex = node_responde_time[i];
			int nodeIndex = atoi(string_nodeIndex.c_str());
			if(storages[nodeIndex]->check_health()==0)
			{
				healthy_nodes.push_back(nodeIndex);
			}
			else
			{
				print_error(
							stringstream() << "WARNING: node " << nodeIndex << " may be down."
											<< endl);
			}
		}
		if (healthy_nodes.size() < (unsigned int) coding->getk()) {
						print_error(stringstream() << "Insufficient healthy nodes." << endl);
						exit(-1);
					}
	}
	/*
	 * 如果不能通过node_responde_time，那么按照原来的方法
	 */
	else
	{
		// look for healthy nodes for the sorted nodes
			int n = coding->getn();

			for (int i = 0; i < n; ++i) { //这里首先要检查某个节点是否为health（这里是否可以进行简单设置）
				if (storages[i]->check_health() == 0) {
					healthy_nodes.push_back(i);
				} else {
					print_error(
							stringstream() << "WARNING: node " << i << " may be down."
									<< endl);
				}
			}

			unsigned int k = (unsigned int) coding->getk();
			if (healthy_nodes.size() < k) {
				print_error(stringstream() << "Insufficient healthy nodes." << endl);
				exit(-1);
			}

	}


	  // download chunks from the first k healthy node, and save their chunk indices
	  vector<int> chunk_indices;
	  unsigned int k = (unsigned int) coding->getk();
	  for (unsigned int i=0; i<k; ++i) {
	    vector<int> cur_chunk_indices;
	    coding->chunks_on_node(healthy_nodes[i], cur_chunk_indices);
	    chunk_indices.insert(chunk_indices.end(),
	                         cur_chunk_indices.begin(), cur_chunk_indices.end());
	  }


	// create job 1: download_chunks()
	Job *job1 = new Job(Job::DLCHUNKS, coding, &storages, tmpdir, filename);
	job1->chunk_indices = chunk_indices;
	for (unsigned int i = 0; i < k; ++i) {
		job1->node_indices.push_back(healthy_nodes[i]);
	}

	// create job 2: download_metadata()
	Job *job2 = new Job(Job::DLMETA, coding, &storages, tmpdir, filename);
	job2->node_indices.push_back(healthy_nodes[0]);

	// create job 3: decode_file()
	Job *job3 = new Job(Job::DECODE, coding, &storages, tmpdir, filename);
	job3->chunk_indices = chunk_indices;

	// chain the jobs and enqueue job 1 [download_chunks()]
	job1->next_job = job2;
	job2->next_job = job3;
	add_job(job1, storage_queue, master_mutex, storage_queue_ready);

}

void FileOp::repair_file(string &filename, Coding *coding,
		vector<Storage *> &storages, vector<int> &chunks_to_retrieve,
		int faulty_node, string &tmpdir) {
	print(stringstream() << "Repairing: " << filename << endl);

	// create job 1: download_chunks()
	// (metadata already downloaded during preprocess)
	Job *job1 = new Job(Job::DLCHUNKS, coding, &storages, tmpdir, filename);
	job1->chunk_indices = chunks_to_retrieve;
	bool *node_indices = new bool[coding->getn()]();
	for (auto chunk_index : chunks_to_retrieve) {
		node_indices[coding->nodeid(chunk_index)] = true;
	}
	for (int i = 0; i < coding->getn(); ++i) {
		if (node_indices[i] == true) {
			job1->node_indices.push_back(i);
		}
	}
	delete[] node_indices;

	// create job 2: repair_file()
	Job *job2 = new Job(Job::REPAIR, coding, &storages, tmpdir, filename);

	// create job 3: upload_metadata_and_chunks() for new node
	Job *job3 = new Job(Job::ULMETACHUNKS, coding, &storages, tmpdir, filename);
	job3->node_indices.push_back(faulty_node);
	coding->chunks_on_node(faulty_node, job3->chunk_indices);

	// create job 4: upload_metadata() for surviving nodes
	Job *job4 = new Job(Job::ULMETA, coding, &storages, tmpdir, filename);
	for (int i = 0, j = 0; i < coding->getn(); ++i) {
		if (i == faulty_node) {
			j += coding->chunks_per_node();
			continue;
		}
		job4->node_indices.push_back(i);
		for (int jj = 0; jj < coding->chunks_per_node(); ++jj, ++j) {
			job4->chunk_indices.push_back(j);
		}
	}

	// chain the jobs and enqueue job 1 [download_chunks()]
	job1->next_job = job2;
	job2->next_job = job3;
	job3->next_job = job4;
	add_job(job1, storage_queue, master_mutex, storage_queue_ready);
}

void FileOp::delete_file(string &filename, Coding *coding,
		vector<Storage *> &storages) {
	print(stringstream() << "Deleting: " << filename << endl);

	int n = coding->getn();
	for (int i = 0; i < n; ++i) {
		vector<int> chunk_indices;
		coding->chunks_on_node(i, chunk_indices);
		if (storages[i]->delete_metadata_and_chunks(filename, chunk_indices)
				== -1) {
			print_error(
					stringstream() << "Failed to delete " << filename
							<< " from node " << i << endl);
			exit(-1);
		}
	}
}

/*  -----------------  */
/* | Private methods | */
/*  -----------------  */
FileOp::FileOp() {
	// spawn one master storage thread and one master coding thread
	// TODO: consider spawning sub-threads within each of the master thread in the future
	/*
	 * 注意这个master_mutex是全局锁
	 * 在workers中添加两个线程 分别 存取操作和解码操作
	 */
	workers.push_back(
			thread(run_thread, ref(storage_queue), ref(master_mutex),
					ref(storage_queue_ready)));
	workers.push_back(
			thread(run_thread, ref(coding_queue), ref(master_mutex),
					ref(coding_queue_ready)));
}

