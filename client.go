package main

import (
	"flag"
	"fmt"
	"math/rand"
	"net"
	"time"
)

var (
	connNum *int
	server  *string
	port    *string
)

const (
	defConnNum   = 20000
	defSendBytes = 64
	defServer    = "127.0.0.1"
	defPort      = "7788"
)

const letterBytes = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"

func init() {
	connNum = flag.Int("c", defConnNum, "echo client number")
	server = flag.String("s", defServer, "echo server address")
	port = flag.String("p", defPort, "echo server port")

	rand.Seed(time.Now().UnixNano())
}

func echoRandomBuf(size int) []byte {
	b := make([]byte, size)

	for i := range b {
		b[i] = letterBytes[rand.Intn(len(letterBytes))]
	}

	return b
}

func echoClient(ch chan<- struct{}) {
	var (
		recvBuf [defSendBytes]byte
		sendBuf []byte
		n       int
		pos     int
		err     error
	)

	conn, err := net.Dial("tcp", *server+":"+*port)
	if err != nil {
		fmt.Printf("Dial %s:%s failed\n", *server, *port)
		goto done
	}

	defer conn.Close()

	sendBuf = echoRandomBuf(defSendBytes)

	n, err = conn.Write(sendBuf)
	if err != nil || n != defSendBytes {
		fmt.Printf("Conn write failed\n")
		goto done
	}

	pos = 0
	for pos < defSendBytes {
		n, err = conn.Read(recvBuf[pos:])
		if err != nil {
			fmt.Printf("Conn read failed\n")
			goto done
		}

		pos += n
	}

done:
	ch <- struct{}{}
	return
}

func main() {
	flag.Parse()

	chs := make(chan struct{}, *connNum)

	for i := 0; i < *connNum; i++ {
		go echoClient(chs)
	}

	for i := 0; i < *connNum; i++ {
		<-chs
	}

	fmt.Printf("All done!\n")

}
