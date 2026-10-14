;;;;	-------------------------------
;;;;	Copyright (c) Corman Technologies Inc.
;;;;	See LICENSE.txt for license information.
;;;;	-------------------------------
;;;;
;;;;	File:		bsd-sockets.lisp
;;;;	Contents:	The sockets of the C library, under the names which
;;;;				winsock.lisp gives those of Windows.
;;;;
;;;;	SOCKETS is written against the package WINSOCK. Where the system is
;;;;	not Windows, this file is that package: the same names, with the
;;;;	values, the structures and the functions of the system.
;;;;

(defpackage "WINSOCK"
  (:use "COMMON-LISP" "C-TYPES")
  (:shadow "LISTEN"))
(in-package :winsock)

(export '(*addr-size* c-to-addr addr-to-c get-addr-info get-name-info address-family-available-p
		  closesocket ioctlsocket WSAGetLastError))

#! (:export t :library "libc.so.6")
#define INVALID_SOCKET		-1
#define SOCKET_ERROR		-1

#define AF_UNSPEC			0
#define AF_INET				2
#define AF_INET6			10

#define SOCK_STREAM			1
#define SOCK_DGRAM			2

#define SOMAXCONN			4096

#define AI_PASSIVE			0x1
#define AI_CANONNAME		0x2
#define AI_NUMERICHOST		0x4

#define NI_MAXHOST			1025
#define NI_MAXSERV			32
#define NI_NUMERICHOST		0x01
#define NI_NUMERICSERV		0x02
#define NI_NOFQDN			0x04
#define NI_NAMEREQD			0x08
#define NI_DGRAM			0x10

#define FIONREAD			0x541B
#define FIONBIO				0x5421

#define MSG_NOSIGNAL		0x4000

struct in_addr {
	unsigned long S_addr;
};

struct sockaddr {
	unsigned short sa_family;
	char sa_data[14];
};

struct sockaddr_in {
	short sin_family;
	unsigned short sin_port;
	in_addr sin_addr;
	char sin_zero[8];
};

struct in6_addr {
	unsigned short Word[8];
};

struct sockaddr_in6 {
	short sin6_family;
	unsigned short sin6_port;
	unsigned long sin6_flowinfo;
	in6_addr sin6_addr;
	unsigned long sin6_scope_id;
};

/* the order of ai_addr and ai_canonname is not that of Windows */
typedef struct addrinfo {
	int ai_flags;
	int ai_family;
	int ai_socktype;
	int ai_protocol;
	unsigned int ai_addrlen;
	sockaddr *ai_addr;
	char *ai_canonname;
	void *ai_next;
} ADDRINFO;

int accept(int s, sockaddr *addr, int *addrlen);
int bind(int s, const sockaddr *addr, int namelen);
int connect(int s, const sockaddr *name, int namelen);
int getpeername(int s, sockaddr *name, int *namelen);
int getsockname(int s, sockaddr *name, int *namelen);
int getsockopt(int s, int level, int optname, char *optval, int *optlen);
int setsockopt(int s, int level, int optname, const char *optval, int optlen);
int listen(int s, int backlog);
int recv(int s, char *buf, int len, int flags);
int recvfrom(int s, char *buf, int len, int flags, sockaddr *from, int *fromlen);
int shutdown(int s, int how);
int socket(int af, int type, int protocol);
unsigned long htonl(unsigned long hostlong);
unsigned short htons(unsigned short hostshort);
unsigned long ntohl(unsigned long netlong);
unsigned short ntohs(unsigned short netshort);
int gethostname(char *name, int namelen);
int getaddrinfo(const char *nodename, const char *servname, const addrinfo *hints, addrinfo **res);
void freeaddrinfo(addrinfo *ai);
int getnameinfo(const sockaddr *sa, int salen, char *host, unsigned long hostlen,
	char *serv, unsigned long servlen, int flags);
!#

(ct:defun-dll closesocket ((s :long))
   :return-type :long
   :library-name cl::c-library
   :entry-name "close"
   :linkage-type :c)

(ct:defun-dll ioctlsocket ((s :long) (cmd :unsigned-long) (argp (:unsigned-long *)))
   :return-type :long
   :library-name cl::c-library
   :entry-name "ioctl"
   :linkage-type :c)

;;; Writing to a connection which the other end has closed is an error, as
;;; on Windows, rather than a signal which ends the process.
(ct:defun-dll %send ((s :long) (buf (:char *)) (len :long) (flags :long))
   :return-type :long
   :library-name cl::c-library
   :entry-name "send"
   :linkage-type :c)

(ct:defun-dll %sendto ((s :long) (buf (:char *)) (len :long) (flags :long)
		(to (sockaddr *)) (tolen :long))
   :return-type :long
   :library-name cl::c-library
   :entry-name "sendto"
   :linkage-type :c)

(defun send (s buf len flags)
	(%send s buf len (logior flags MSG_NOSIGNAL)))

(defun sendto (s buf len flags to tolen)
	(%sendto s buf len (logior flags MSG_NOSIGNAL) to tolen))

(export '(send sendto))

(ct:defun-dll errno-location ()
   :return-type (:long *)
   :library-name cl::c-library
   :entry-name "__errno_location"
   :linkage-type :c)

;;; The error of the last call which failed, in this thread.
(defun WSAGetLastError ()
	(cref (:long *) (errno-location) 0))

(defvar *addr-size* 28)

(defun clear-mem (buffer size) (dotimes (n size) (setf (cref (byte *) buffer n) 0)))

(defun c-to-addr (sockaddr)
    (let ((dotted (malloc 64)))
        (if (eq (cref (:short *) sockaddr *) AF_INET)
            (with-c-struct (ignore sockaddr sockaddr_in)
                (getnameinfo sockaddr 16 dotted 64 null 0 NI_NUMERICHOST)
                (list :family 'af_inet
                       :port (ntohs sin_port)
                       :ipaddr (c-bytes-to-lisp-bytes sin_addr 4)
                       :dotted (c-string-to-lisp-string dotted)))
            (with-c-struct (ignore sockaddr sockaddr_in6)
                (getnameinfo sockaddr 28 dotted 64 null 0 NI_NUMERICHOST)
                (list :family 'af_inet6
                       :port (ntohs sin6_port)
                       :flow sin6_flowinfo
                       :ipaddr (c-bytes-to-lisp-bytes sin6_addr 16)
                       :scope sin6_scope_id
                       :dotted (c-string-to-lisp-string dotted))))))

(defun addr-to-c (addr)
    (let ((family (symbol-value (getf addr :family))) (c (malloc *addr-size*)))
        (clear-mem c *addr-size*)
        (if (eq family AF_INET)
            (with-c-struct (ignore c sockaddr_in)
                (setq sin_family family sin_port (htons (getf addr :port)))
                (lisp-bytes-to-c-bytes (getf addr :ipaddr) sin_addr))
            (with-c-struct (ignore c sockaddr_in6)
                (setq sin6_family family sin6_port (htons (getf addr :port))
                         sin6_flowinfo (or (getf addr :flow) 0) sin6_scope_id (or (getf addr :scope) 0))
                (lisp-bytes-to-c-bytes (getf addr :ipaddr) sin6_addr))) c))

;;; The length of the address which ADDR-TO-C makes, for the functions which
;;; are given one: Linux wants that of the family's structure.
(defun addr-size (addr)
	(if (eq (symbol-value (getf addr :family)) AF_INET) 16 28))

(defun get-addr-info (host &key (port 0) host-is-name ipv6 errorp)
    "Port: symbol, string or integer. Host-is-name if host string is a host name: nil, :unspec or t. IPv6: nil, :only or t."
    (let ((hint (malloc (sizeof 'addrinfo))) (p (malloc (sizeof 'win:pvoid))) pp result)
        (clear-mem hint (sizeof 'addrinfo))
        (with-c-struct (ignore hint addrinfo)
            (setq ai_flags (if (eq host-is-name t) 0 AI_NUMERICHOST)
                     ai_family (if (eq ipv6 t) AF_UNSPEC (if (not ipv6) AF_INET AF_INET6))))
        (setq result (getaddrinfo (lisp-string-to-c-string host) (lisp-string-to-c-string (format nil "~a" port)) hint p))
        (if (zerop result)
            (setq pp (cref (win:pvoid *) p *))
            (when (and errorp (not (eq host-is-name :unspec))) (error "Socket error ~A." result)))
        (or (and pp (do ((p pp (cref addrinfo p ai_next)) res) ((cpointer-null p) (freeaddrinfo pp) (nreverse res))
                              (push (c-to-addr (cref addrinfo p ai_addr)) res)))
             (and (eq host-is-name :unspec) (get-addr-info host :port port :host-is-name t :ipv6 ipv6 :errorp errorp)))))

(defun get-name-info (addr &key dottedp portp errorp) "Returns host and port names as values."
    (let* ((host-text (malloc NI_MAXHOST))
		   (serv-text (if portp (malloc NI_MAXSERV) null))
		   (result (getnameinfo (addr-to-c addr) (addr-size addr) host-text NI_MAXHOST serv-text NI_MAXSERV
						(logior (if dottedp NI_NUMERICHOST 0) (if errorp NI_NAMEREQD 0)))))
        (if (zerop result)
            (values (c-string-to-lisp-string host-text)
                    (when portp (setq serv-text (c-string-to-lisp-string serv-text)) (unless (numberp (parse-integer serv-text :junk-allowed t)) serv-text)))
            (when errorp (error "Socket error ~A." result)))))

(defun address-family-available-p (address-family)
	(let ((s (socket address-family SOCK_STREAM 0)))
		(and (>= s 0) (progn (closesocket s) t))))

(provide "WINSOCK")
