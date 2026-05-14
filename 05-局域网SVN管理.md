SVN服务器地址: 192.168.1.77
用户名: root
密码: cetcA123@

https协议访问管理
配置文件路径: /repo/svn/conf/
修改用户和权限: /repo/svn/conf/auth
新增用户密码: htpasswd /repo/svn/conf/httpd-passwd new-user
删除用户密码: htpasswd -D /repo/svn/conf/httpd-passwd delete-user

修改完配置之后重启apache服务，在Ubuntu下为apache，在Centos下为httpd
(sudo) systemctl restart httpd