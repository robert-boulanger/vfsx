#
# This module contains classes for implementing a custom VFSX handler.
#
# Copyright (C) 2004 Steven R. Farley.  All rights reserved.
#
import pydevd

pydevd.settrace('10.0.1.214', port=31313, stdoutToServer=True, stderrToServer=True)
import sys
import os
import os.path
import SocketServer
import logging

# General error.  VFS operation should not proceed.
FAIL_ERROR = -1

# Authorization error.  User is not allowed to execute operation.
# VFS operation should not proceed.
FAIL_AUTHORIZATION = -2

# Operation not implemented.
FAIL_NOT_IMPLEMENTED = -3

# Successful operation.  VFS operation can proceed.
SUCCESS_TRANSPARENT = 0

# The Unix domain socket file
SOCKET_FILE = "/tmp/vfsx-socket"

# Logger for this module
logging.basicConfig()
log = logging.getLogger("vfsx")
log.setLevel(logging.DEBUG)


class VFSOperationResult(object):

    def __init__(self, status):
        self.status = status


class VFSModuleSession(object):
    # The VFSModuleSession subclass of new instances created
    # in getSession()
    __sessionClass = None

    # All connected sessions
    __sessions = {}

    def setSessionClass(sessionClass):
        VFSModuleSession.__sessionClass = sessionClass

    setSessionClass = staticmethod(setSessionClass)

    def getSessionClass():
        return VFSModuleSession.__sessionClass

    getSessionClass = staticmethod(getSessionClass)

    def getSession(origpath):
        sessions = VFSModuleSession.__sessions
        key = origpath
        if sessions.has_key(key):
            session = sessions[key]
            log.debug("Existing session: %s" % session)
        else:
            session = VFSModuleSession.__sessionClass(key)
            sessions[key] = session
            log.debug("New session: %s" % session)
        return session

    getSession = staticmethod(getSession)

    def removeSession(session):
        key = session.origpath
        del VFSModuleSession.__sessions[key]
        log.debug("Removed session: %s" % session)

    removeSession = staticmethod(removeSession)

    # Instance methods

    def __init__(self, origpath):
        self.origpath = origpath

    def __str__(self):
        return "origpath = %s" % (self.origpath)

    # This is the default method to call when a specific operation method is
    # not found on the subclass.  The subclass can override this method to
    # return a different result.
    def defaultOperation(self, *args):
        return VFSOperationResult(FAIL_NOT_IMPLEMENTED)

    # VFS disk operations

    def connect(self):
        return VFSOperationResult(SUCCESS_TRANSPARENT)

    def disconnect(self):
        return VFSOperationResult(SUCCESS_TRANSPARENT)

    # VFS directory operations

    def opendir(self, path):
        return VFSOperationResult(SUCCESS_TRANSPARENT)

    def mkdir(self, path, mode):
        VFSOperationResult(SUCCESS_TRANSPARENT)

    def rmdir(self, path):
        return VFSOperationResult(SUCCESS_TRANSPARENT)

    # VFS file operations

    def open(self, path, flags, mode):
        return VFSOperationResult(SUCCESS_TRANSPARENT)

    def close(self, path):
        return VFSOperationResult(SUCCESS_TRANSPARENT)


    def create(self, path):
        return VFSOperationResult(SUCCESS_TRANSPARENT)

    def read(self, path):
        return VFSOperationResult(SUCCESS_TRANSPARENT)

    def pread(self, path):
        return VFSOperationResult(SUCCESS_TRANSPARENT)

    def write(self, path):
        return VFSOperationResult(SUCCESS_TRANSPARENT)

    def pwrite(self, path):
        return VFSOperationResult(SUCCESS_TRANSPARENT)

    def lseek(self, path):
        return VFSOperationResult(SUCCESS_TRANSPARENT)

    def rename(self, oldPath, newPath):
        return VFSOperationResult(SUCCESS_TRANSPARENT)

    def unlink(self, path):
        return VFSOperationResult(SUCCESS_TRANSPARENT)


# Expects a string of the form "operation:origpath:arg1,arg2,arg3" where
# "operation" is the VFS operation.  It should exactly match the method on
# VFSModuleSession.  A new VFSModuleSession is created for each "connect"
# operation.
class VFSHandler(SocketServer.BaseRequestHandler):

    def handle(self):
        log.debug("-- Open Connection --")
        while True:
            msg = self.request.recv(512)
            if not msg: break
            log.debug(msg)
            # Handle message-parsing and operation execution error here.
            # Socket communication errors should be propagated.
            try:
                (operation, origpath, args) = self.__parseMessage(msg)
                result = self.__callOperation(operation, origpath, args)
            except Exception, e:
                result = VFSOperationResult(FAIL_ERROR)
                log.exception(e)
            # sys.stderr.write("%s\n" % msg)
            self.request.send("%d" % 0)

        # The client probably closed the connection.
        self.request.close()
        log.debug("Close Connection")

    def __parseMessage(self, msg):
        parts = msg.split(":")
        (operation, origpath) = parts[0:2]
        log.debug("  operation = '%s' origpath = '%s'" %
                  (operation, origpath))
        args = []
        if len(parts) > 3:
            args = parts[2].split(",")
            log.debug("  args = '%s'" % parts[2])
        return (operation, origpath, args)

    def __callOperation(self, operation, origpath, args):
        session = VFSModuleSession.getSession(origpath)
        if operation == "disconnect":
            VFSModuleSession.removeSession(session)
        sessionClass = VFSModuleSession.getSessionClass()
        try:
            method = eval("sessionClass.%s" % operation)
        except AttributeError:
            method = sessionClass.defaultOperation
        return method(session, *args)


def runServer(vfsSessionClass):
    log.info("Starting socket server using session class '%s.%s'"
             % (vfsSessionClass.__module__, vfsSessionClass.__name__))
    if os.path.exists(SOCKET_FILE):
        os.unlink(SOCKET_FILE)
    VFSModuleSession.setSessionClass(vfsSessionClass)
    server = SocketServer.UnixStreamServer(SOCKET_FILE, VFSHandler)
    try:
        server.serve_forever()
    except Exception, e:
        log.info("Socket server stopped due to exception '%s'" % e.__class__)


# Initialize default VFSModuleSession class
VFSModuleSession.setSessionClass(VFSModuleSession)

if __name__ == "__main__":

    # If no args given, handle requests with the base VFSModuleSession class.
    if len(sys.argv) == 1:
        runServer(VFSModuleSession)
    # If 2 args are provided, treat them as the module name and class name of
    # the VFSModuleSession subclass to use for handling requests.
    else:
        modulename = sys.argv[1]
        clsname = sys.argv[2]
        module = __import__(modulename, globals(), locals(), [clsname])
        cls = vars(module)[clsname]
        runServer(cls)
