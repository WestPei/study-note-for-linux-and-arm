#!/bin/sh
current_path=`pwd`
log_path=/var/log
file_max=50
disk_max=512
archive_path=$log_path/archive
del_flag=0
del_sum=$((file_max /5))
current_time=`date "+%Y%m%d%H%M%S"`

output_info() {
	echo $1
	logger -p local7.info "[log-manage.sh] $1"
}

mkdir -p $archive_path

log_count=`ls $archive_path|grep -c "\.tar\.gz"`
if [ $log_count -gt $file_max ];then
	del_flag=1
fi

dir_size=`du -sm $archive_path|awk '{print $1}'` 
if [ $dir_size -gt $disk_max ];then
    del_flag=1
fi

if [ $del_flag -eq 1 ];then
    output_info "$del_sum achieve log in $archive_path need delete ..."
	cd "$archive_path" && ls -rt $archive_path |grep "\.tar\.gz"| head -n $del_sum | xargs rm -f
	cd $current_path
fi


tar -zcvf $archive_path/wap_$current_time.tar.gz $log_path --exclude=archive --exclude=lost+found
if [ $? -eq 0 ];then
	find $log_path/rsyslog -type f -name "*.gz" -exec rm -rf {} \;
	find $log_path/journal -type f -name "system@*" -exec rm -rf {} \;
	rm -rf $log_path/coredump/*
	rm -rf $log_path/access.log
	rm -rf $log_path/lighttpd.error.log
	rm -rf $log_path/install.log
fi