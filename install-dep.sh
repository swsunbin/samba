function samba_dep()
{
    dnf --enablerepo=powertools install -y avahi-devel
    yum install cups-devel -y
    yum install dbus-devel -y
    yum install e2fsprogs-devel -y
    yum install jansson-devel -y
    yum install readline-devel -y
    dnf --enablerepo=powertools install -y rpcsvc-proto-devel
     dnf --enablerepo=powertools install lmdb-devel
     dnf --enablerepo=powertools install lmdb
     yum install 
     yum libcap-devel 
     yum libattr-devel 
     yum libarchive-devel 
     yum libacl-devel
     yum install python3-dns
     yum install python3-dnsy
    yum install perl-Parse-Yapp.noarch

     dnf --enablerepo=powertools install mingw64-gcc
     dnf --enablerepo=powertools install mingw32-gcc
     dnf --enablerepo=powertools install rpcgen
     dnf --enablerepo=powertools install libcmocka-devel
     dnf --enablerepo=powertools install glusterfs-api-devel
     dnf --enablerepo=powertools install gpgme-devel
     dnf --enablerepo=powertools install quota-devel
     dnf --enablerepo=powertools install libtasn1-tools
     dnf --enablerepo=powertools install liburing-devel
     dnf --enablerepo=powertools install pam-devel
}

function talloc_dep()
{
    sudo yum install -y python-devel    # for python2.x installs
    sudo yum install -y python3-devel   # for python3.x installs
}

samba_dep
talloc_dep