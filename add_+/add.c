#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#ifdef __APPLE
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#define Chr_error() printf("============>%d\n",__LINE__)

const int ARRAY_SIZE =1000;

//clGetProgramInfo
int SaveProgramToBinary(cl_program program,const char *fileName)
{
	cl_uint err;
	cl_uint numDevices =0;
	err = clGetProgramInfo(program,CL_PROGRAM_NUM_DEVICES,sizeof(cl_uint),&numDevices,NULL);
	cl_device_id *devices = (cl_device_id*)malloc(sizeof(cl_device_id)*numDevices);
	err = clGetprogramInfo(program,CL_PROGRAM_DEVICES,sizeof(cl_device_id)*numDevices,devices,NULL);
	size_t *programBinarySize=(size_t *)malloc(sizeof(size_t)*numDevices);
	err= clGetProgramInfo(program,CL_PROGRAM_BINARY_SIZES,sizeof(size_t)*numDevices,programBinarySize,NULL);
	unsigned char **programBinaries = (unsigned char **)malloc(sizeof(unsigned char )*numDevices);
	for(cl_uint i =0;i < numDevices;i++)
	{
		programBinaries[i] = (unsigned char **)malloc(sizeof(unsigned char )*programBinarySize[i]);

	}
	err = clGetProgram(program,CL_PROGRAM_BINARIES,sizeof(unsigned char *)*numDevices,
			programBinaries,NULL);

	for(cl_uint i =0;i <numDevices;i++){
		FILE *fp = fopen(fileName,"a+ ");
		fwrite(programBinaries[i],1,programBinarySize[i],fp);
		fclose(fp);
	}

}
char *ReadKernelSourceFile(const char *filename,size_t *length)
{
	FILE *file =NULL;
	size_t sourceLength;
	char *sourceString;
	int ret ;
	file= fopen(filename,"rb");
	if(file ==NULL)
	{
		printf("%s t %d;Can,t opeb %s\n",__FILE__,__LINE__-2,filename);
		return NULL;
	}
	fseek(file,0,SEEK_END);
	sourceLength=ftell(file);
	fseek(file,0,SEEK_SET);
	sourceString = (char *)malloc(sourceLength+1);
	sourceString[0]='\0';
	ret = fread(sourceString,sourceLength,1,file);
	if(ret == 0)
	{
		printf("%s at %d:can't open %s\n",__FILE__,__LINE__);
	}
	fclose(file);
	if(length!=0)
	{
		*length = sourceLength;
	}
	sourceString[sourceLength] = '\0';
	return sourceString;
}

/*1.创建平台
 *2.创建设备
 *3.根据设备创建上下文
 * */

cl_context CreateContext(cl_device_id *device)
{
	cl_int errNum;
	cl_uint numPlatforms;
	cl_uint NumDevice;
	cl_platform_id firstPlatformId;
	cl_context context =NULL;
	errNum = clGetPlatformIDs(1,&firstPlatformId,&numPlatforms);
	if((errNum!= CL_SUCCESS||numPlatforms <=0))
	{
		printf("Faild to find any OpenCL platforms.\n");
		return NULL;
	}
	errNum =clGetDeviceIDs(firstPlatformId,CL_DEVICE_TYPE_CPU,1,device,NULL);
	if(errNum !=CL_SUCCESS)
	{
		printf("this is no Gpu,try CPU....\n");
	}
	if(errNum != CL_SUCCESS)
	{
		printf("there is NO GPU or CPU\n");

		return NULL;
	}
	//两种方法的实现
#if 0
	context = clCreateContext(NULL,1,device,NULL,NULL,&errNum);
#else
	cl_context_properties properites[] = {
			CL_CONTEXT_PLATFORM,
			(cl_context_properties)firstPlatformId,0
	};

	context = clCreateContextFromType(properites,CL_DEVICE_TYPE_ALL,NULL,NULL,&errNum);
	NumDevice = 0;
	size_t DeviceSize;
	errNum = clGetContextInfo(context,CL_CONTEXT_NUM_DEVICES,sizeof(cl_uint),&NumDevice,NULL);
	printf("Number of device in context :%d\n",NumDevice);
#endif

	if(errNum != CL_SUCCESS)
	{
		printf("create context error \n");
		return NULL;
	}
	return context;

}

/*在上下文可用的第一个设备额中创建命令队列
 * */

cl_command_queue CreateCommandQueue(cl_context context,cl_device_id device)
{
	cl_int errNum;
	cl_command_queue commandQueue= NULL;
	commandQueue=clCreateCommandQueue(context,device,0,NULL);
	if(commandQueue ==NULL)
	{
		printf("Failed to create commandQueue for device 0\n");
		return NULL;
	}
	return commandQueue;
}
/*读取内核源码创建OpenCL程序
 *
 * */
cl_program CreateProgram(cl_context context,cl_device_id device,
						const char *fileName)
{
	cl_int errNum;
	cl_program program;
	size_t program_length;
	char *const source =ReadKernelSourceFile(fileName,&program_length);
	program =clCreateProgramWithSource(context,1,(const char **)&source,NULL,NULL);
	if(program ==NULL)
	{
		printf("Failed to create CL program form source.\n");
		return NULL;
	}
	//增加编译链接选项:-cl-std、-cl-mad-enable、-Werror
	const char options[] = "-cl-std=CL1.1 -cl-mad-enable -Werror";
	errNum = clBuildProgram(program,1,&device,options,NULL,NULL);
	if(errNum !=CL_SUCCESS)
	{
		char buildLog[16384];
		clGetProgramBuildInfo(program,device,CL_PROGRAM_BUILD_LOG,sizeof(buildLog),buildLog,NULL);
		printf("Error in kernel :%s \n",buildLog);
		clReleaseProgram(program);
		return NULL;
	}

	SaveProgramToBinary(program,"hello.bin");
	return program;

}
/*创建内存对象
 * */

bool CreateMemObjects(cl_context context,cl_mem  memObjects[3],float *a,float*b)
{
	memObjects[0] = clCreateBuffer(context,CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,sizeof(float)*ARRAY_SIZE,a,NULL);
	memObjects[1] = clCreateBuffer(context,CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,sizeof(float)*ARRAY_SIZE,b,NULL);
	memObjects[2] = clCreateBuffer(context,CL_MEM_READ_WRITE,sizeof(float)*ARRAY_SIZE,NULL,NULL);
	if(memObjects[0] == NULL||memObjects[1]==NULL||memObjects[2]==NULL)
	{
		printf("Error creating memory objects.\n");
		return false;
	}
	return true;
}
/* 清除OpenCL资源
 * */
void Cleanup(cl_context context,cl_command_queue commandQueue,cl_program program,cl_kernel kernel,
		cl_mem memObjects[3])
{
	for(int i = 0; i < 3;i++)
	{
		if(memObjects[i]!=0)
			clReleaseMemObject(memObjects[i]);
	}

	if(commandQueue !=0)
		clReleaseCommandQueue(commandQueue);
	if(kernel !=0)
		clReleaseKernel(kernel);
	if(program !=0)
		clReleaseProgram(program);
	if(context !=0)
		clReleaseContext(context);
}

int main(int argc,char **argv)
{
	cl_context context =0;
	cl_command_queue commandQueue =0;
	cl_program program =0;
	cl_device_id device =0;
	cl_uint numPlatforms = 0;
	cl_kernel kernel =0;
	cl_mem memObjects[3]={0,0,0};
	cl_int errNum = 0;
	cl_platform_id firstPlatformId;
#if 1
	context = CreateContext(&device);
	if(context ==NULL)
	{
		printf("Failed to create OpenCL context.\n");
		return 1;
	}
#else
	errNum = clGetPlatformIDs(1,&firstPlatformId,&numPlatforms);
	if((errNum!= CL_SUCCESS||numPlatforms <=0))
	{
		printf("Faild to find any OpenCL platforms.\n");
		return 1;
	}
	cl_uint num_devices;
	errNum =clGetDeviceIDs(firstPlatformId,CL_DEVICE_TYPE_CPU,1,&device,NULL);
	if(errNum !=CL_SUCCESS)
	{
		printf("this is no Gpu,try CPU....\n");
		return 1;
	}

	context = clCreateContext(NULL,1,&device,NULL,NULL,&errNum);
	if(errNum != CL_SUCCESS)
	{
		printf("create context error \n");
		return 1;
	}
#endif
#if 0
	// 获取OpenCL设备，并创建命令
	commandQueue=clCreateCommandQueue(context,device,0,NULL);
	if(commandQueue ==NULL)
	{
		printf("create commandqueue  error \n");
		Cleanup(context,commandQueue,program,kernel,memObjects);
		return 1;
	}
#else
	// 获取OpenCL设备，并创建命令
	commandQueue = CreateCommandQueue(context,device);
	if(commandQueue ==NULL)
	{
		Cleanup(context,commandQueue,program,kernel,memObjects);
		return 1;
	}
#endif
	//创建Opencl程序
	program = CreateProgram(context,device,"add.cl");
	if(program ==NULL)
	{
		printf("create program  error \n");
		Cleanup(context,commandQueue,program,kernel,memObjects);
		return 1;
	}

	//创建OpenCL内核
	kernel = clCreateKernel(program,"vector_add",NULL);
	if(kernel ==NULL)
	{
		printf("Failed to create kernel");
		Cleanup(context,commandQueue,program,kernel,memObjects);
		return 1;
	}
	//创建内存对象
	float result[ARRAY_SIZE];
	float a[ARRAY_SIZE];
	float b[ARRAY_SIZE];
	for(int i =0; i < ARRAY_SIZE;i++)
	{
		a[i] = (float)i;
		b[i] = (float)(i*2);
	}
	if(!CreateMemObjects(context,memObjects,a,b))
	{
		Cleanup(context,commandQueue,program,kernel,memObjects);
		return 1;
	}
	//设置内核参数
	errNum = clSetKernelArg(kernel,0,sizeof(cl_mem),&memObjects[0]);
	errNum |= clSetKernelArg(kernel,1,sizeof(cl_mem),&memObjects[1]);
	errNum |= clSetKernelArg(kernel,2,sizeof(cl_mem),&memObjects[2]);
	if(errNum !=CL_SUCCESS)
	{
		printf("Error setting kernel argumemts.\n");
		Cleanup(context,commandQueue,program,kernel,memObjects);
		return 1;
	}
	size_t globalWorkSize[1] = {ARRAY_SIZE};
	size_t localWorkSize[1]={1};
	errNum = clEnqueueNDRangeKernel(commandQueue,kernel,1,NULL,globalWorkSize,localWorkSize,0,NULL,NULL);
	if(errNum !=CL_SUCCESS)
	{
		printf("err queuing kernel for execution.\n");
		Cleanup(context,commandQueue,program,kernel,memObjects);
		return 1;
	}
	errNum = clEnqueueReadBuffer(commandQueue,memObjects[2],CL_TRUE,0,ARRAY_SIZE*sizeof(float),result,0,NULL,NULL);
	if(errNum != CL_SUCCESS)
	{
		printf("Error reading result buffer.\n");
		Cleanup(context,commandQueue,program,kernel,memObjects);
	}
	for(int i = 0;i < ARRAY_SIZE;i++)
	{
		printf("i = %d:%f\n",i,result[i]);
	}
	printf("Executed program succesfully.\n");
	Cleanup(context,commandQueue,program,kernel,memObjects);
	return 0;
}

