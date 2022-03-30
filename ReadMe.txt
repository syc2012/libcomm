[ Directory Structure ]

$(BUILD_DIR)
  |-- application (named pipe tcp proxy)
  |-- include     (include header path)
  |-- lib         (link library path)
  |-- source      (source code)
  `-- test        (example programs)


[ Source Code ]

comm_fifo.c
  Named pipe for inter-process communication.

comm_ipc_dgram.c comm_ipc_stream.c
  UNIX domain socket for inter-process communication.

comm_netlink.c
  Netlink socket for user and kernel space communication.

comm_raw.c
  Raw socket for network directly communication.

comm_tcp_client.c comm_tcp_server.c
  TCP socket for network communication.

comm_uart.c
  /dev/ttySx for serial port communication.

comm_udp.c
  UDP socket for network communication.


[ Named Pipe TCP Proxy ]

nptp_conf.xml
  Configuration file that defines the connection mapping talbe.

    <MAPPING enable="1">
        <PORT>TCP prot number</PORT>
        <PIPE>/path_name/IPC_stream_name</PIPE>
        <DESC>Description string</DESC>
    </MAPPING>


[ Build Command ]

$ source setup.gcc
$ make
