#include "tsserver.h"
#include "hlsenc.h"

//http://blog.csdn.net/roland_sun/article/details/46049485
//valgrind --leak-check=full --track-origins=yes --log-file=/data/local/log.txt /data/local/hls

int main(int argc ,char ** argv)
{
	av_register_all();
	avformat_network_init();
	
	start_tsserver("8181");
	
	//char *url = "http://zb.gxtv.cn/live/nn_live1007.flv?wsSecret=ecacce2b1de71a7a6ed943ad0d06b10e&wsTime=591961dd";
	//char *url = "http://223.255.29.9/zhcs/live/cstv20_300k/live.m3u8";
	char *url = "http://120.87.4.70/PLTV/88888894/224/3221225486/3.m3u8";
	//char *url = "http://cctv1.vtime.cntv.cloudcdn.net/live/cctv1hls_/index.m3u8?ptype=1&amode=1&AUTH=5oULA/QUFVxKnmPb8DketEkm3Fn5VbAzzSdmbEuHM+mA8OqWsLxH67GebqDqXHe2WQAb4SXYG2njygdQjrZLUg==";
	//char *url = "http://weblive.hebtv.com/live/hbws_bq/index.m3u8";
	//char *url = "rtmp://livetv.dhtv.cn/live/financial";
	//char *url = "rtmp://60.214.132.52/tslsChannelLive/pr3Qevo/live";
	//char *url = "http://live.hcs.cmvideo.cn:8088/wd_r2/cctv/jiangsuhd/1200/index.m3u8?msisdn=3789254564222&mdspid=&spid=800033&netType=5&sid=5500182273&pid=2028597139&timestamp=20170428131503&Channel_ID=0116_06000000-99000-200300140100000&ProgramID=623539778&ParentNodeID=-99&client_ip=121.43.148.152&assertID=5500182273&imei=111fcd9b089c464ae49f0a4d4bd3ca6f2caffafbf60b963a7c3c2f6e67bd2138&SecurityKey=20170428131503&encrypt=3debc147e2f13daace4479bd9182c68a";

	//char *url = "http://zb.gxtv.cn/live/nn_live1000.flv?wsSecret=706412e8abc1f00352eeb79838472a62&wsTime=59195f67";
	
	//char *url = "http://weblive.hebtv.com/live/hbws_bq/index.m3u8";
	//char *url = "http://rrsm.iptv.gd.cn:30001/PLTV/88888905/224/3221227483/1.m3u8";
	
	HLSContext *hls = init_hlscontex(url);
	put_queuelist(123456, hls->queue);
	
	start_slice(hls);
	
	
	
	return getchar();
}