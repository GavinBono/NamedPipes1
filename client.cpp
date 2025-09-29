/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Gavin Bono
	UIN: 233000849
	Date: 9/28/2025
*/
#include "common.h"
#include "FIFORequestChannel.h"
#include <sys/wait.h>
#include <sys/stat.h>
#include <iomanip>

using namespace std;
//helpers
static FIFORequestChannel* open_new_channel(FIFORequestChannel* control) {
	MESSAGE_TYPE m = NEWCHANNEL_MSG;
	control->cwrite(&m, sizeof(m));
	char namebuf[64] = {0};
	control->cread(namebuf, sizeof(namebuf));
	return new FIFORequestChannel(namebuf, FIFORequestChannel::CLIENT_SIDE);
}
static void packed_file_request(FIFORequestChannel& ch, const std::string& fname, __int64_t offset, int length, void* reply_buf) {
	filemsg fm(offset, length);
	int mlen=sizeof(filemsg)+ fname.size() + 1;
	std::vector<char> req(mlen);
	memcpy(req.data(), &fm, sizeof(filemsg));
	strcpy(req.data() + sizeof(filemsg), fname.c_str());
	ch.cwrite(req.data(), mlen);
	ch.cread(reply_buf, length);
}
static __int64_t request_file_size(FIFORequestChannel& ch, const std::string& fname) {
	__int64_t sz=0;
	filemsg fm(0,0);
	int mlen=sizeof(filemsg) + fname.size() + 1;
	std::vector<char> req(mlen);
	memcpy(req.data(), &fm, sizeof(filemsg));
	strcpy(req.data() + sizeof(filemsg), fname.c_str());
	ch.cwrite(req.data(), mlen);
	ch.cread(&sz, sizeof(sz));
	return sz;
}
static void ensure_dir(const char* path) {
	struct stat st();
	if(stat(path, &st) == -1) {
		mkdir(path, 0777);
	}
}
int main (int argc, char *argv[]) {
	//parse args
	int opt;
	int p = -1;
	double t = -1.0;
	int e = -1;
	buffercapacity = MAX_MESSAGE;
	bool use_new_channel = false;
	std::string filename;
	
	while ((opt = getopt(argc, argv, "p:t:e:f:cm:")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
				break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'f':
				filename = optarg;
				break;
			case 'c':
				use_new_channel=true;
				break;
			case 'm':
				buffercapacity=atoi(optarg);
				break;
		}
	}
	//fork/exec server
	pid_t pid = fork();
	if(pid<0) {
		perror("fork");
		return 1;
	}
	if(pid==0) {
		std::string mstr = std::to_string(buffercapacity);
		char* const argv_srv[] = { (char*)"./server", (char*)"-m", (char*)mstr.c_str(), nullptr };
		execvp(argv_srv[0], argv_srv);
		perror("execvp"); _exit(127);
	}

    FIFORequestChannel control("control", FIFORequestChannel::CLIENT_SIDE);
	
	//create new channel for work
	FIFORequestChannel* work = &control;
	std::unique_ptr<FIFORequestChannel> owned;
	if (use_new_channel) {
		owned.reset(open_new_channel(&control));
		work=owned.get();
	}

	//cases!!!
	//case A, single datapoint
	if(p!=-1 && t >=0.0 && e!=-1 && filename.empty()) {
		datamsg req(p, t, e);
		work->cwrite(&req, sizeof(req));
		double val=0.0;
		work->cread(&val, sizeof(val));
		std::cout << std::fixed << std::setprecision(3);
		std::cout << "For person " << p << ". at time " << t << ", the value of ecg " << e << " is " << val << std::endl;

	}
	//case B, first 1000 rows for a patient 
	else if(p!=-1 && t<0.0 && e==-1 && filename.empty()) {
		std::ofstream ofs("x1.csv");
		ofs << std::fixed << std::setprecision(3);
		for(int i=0; i<1000; ++i) {
			double ts=i * 0.004;
			datamsg r1(p,ts,1), r2(p,ts,2);
			double v1=0, v2=0; 
			work->cwrite(&r1, sizeof(r1)); work->cread(&v1, sizeof(v1));
			work->cwrite(&r2, sizeof(r2)); work->cread(&v2, sizeof(v2));
			ofs << ts << "," << v1 << "," << v2 << "\n";
		}
		ofs.close();
	}
	//case C, file transfer
	else if(!filename.empty()) {
		ensure_dir("received");
		std::string outpath="received/" + filename;
		__int64_t fsize = request_file_size(*work, filename);
		FILE* out = fopen(outpath.c_str(), "wb");
		if (!out) {
			perror("fopen");
			return 1;
		}
		__int64_t offset = 0;
		while(offset < fsize) {
			int chunk = (int_ std::min<__int64_t>(buffercapacity, fsize - offset));
			std::vector<char> reply(chunk);
			packed_file_request(*work, filename, offset, chunk, reply.data());
			fwrite(reply.data(), 1, chunk, out);
			offset+=chunk;
		}
		fclose(out);
	}

	//clean shutdown
	if(work!=&control) {
		MESSAGE_TYPE q = QUIT_MSG;
		work->cwrite(&q, sizeof(q));
		owned.reset();
	}
	{
		MESSAGE_TYPE q = QUIT_MSG;
		control.cwrite(&q, sizeof(q));
	}
	int status=0;
	waitpid(pid, &status, 0);
	return 0;

}
