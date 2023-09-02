



-- device_tb
CREATE TABLE device_tb (id VARCHAR(40) PRIMARY KEY, ip VARCHAR(40) NOT NULL, port int NOT NULL, 
platform VARCHAR(40), type int, protocol int, username VARCHAR(40), password VARCHAR(40), name VARCHAR(40), 
manufacturer VARCHAR(40), firmware VARCHAR(40), model VARCHAR(40), status int, expires int,keepalive int,
max_k_timeout int,channels int);

INSERT INTO device_tb VALUES ('34020000001320000001', '192.168.41.252', 5060, 'GB/T28181-2016', 1, 1, '34020000001320000001', '123456', 'HK-Camera', '', '', '', 0, 3600, 60, 3, 0);
INSERT INTO device_tb VALUES ('34020000001320000002', '192.168.41.156', 5060, 'GB/T28181-2016', 1, 1, '34020000001320000002', '123456', 'HK-Camera', '', '', '', 0, 3600, 60, 3, 0);