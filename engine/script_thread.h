
#ifndef _TA_SCRIPT_THREAD_
#define _TA_SCRIPT_THREAD_

Handle<Value> InitThreadPool(const Arguments& args)
{
	HandleScope handle_scope; //���������Handle(�����˳�ʱ�ͷ�ѭ�����Handle<String> s)
	
	if (args.Length() > 0)
	{
		if (script_tpool.Init(args[0]->Uint32Value()))
		{
			return ::v8::True();
		}
	}
	//std::cout << *(String::AsciiValue(s)) << std::endl;
	return ::v8::False();
}

Handle<Value> DestoryThreadPool(const Arguments& args)
{
	HandleScope handle_scope; //���������Handle(�����˳�ʱ�ͷ�ѭ�����Handle<String> s)

	script_tpool.Destory();
	//std::cout << *(String::AsciiValue(s)) << std::endl;
	return Undefined();
}

void AddThreadFunc(Handle<ObjectTemplate> global)
{
	global->Set(String::New("InitThreadPool"), FunctionTemplate::New(InitThreadPool));
	global->Set(String::New("DestoryThreadPool"), FunctionTemplate::New(DestoryThreadPool));
}

#endif //_TA_SCRIPT_THREAD_