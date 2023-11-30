function samba_dep()
{
    sudo yum install -y perl-Archive-Tar.noarch 
    sudo yum install -y perl-Test-use-ok.noarch
    sudo yum install -y perl-Tk-devel.x86_64
    sudo yum install -y	avahi-devel 
    sudo yum install -y	cups-devel
    sudo yum install -y	docbook-style-xsl
    sudo yum install -y	e2fsprogs-devel
    sudo yum install -y	libtirpc-devel
    sudo yum install -y	popt-devel
    sudo yum install -y	quota-devel 
    sudo yum install -y	readline-devel
    sudo yum install -y	libtasn1-tools
    sudo yum install -y	glusterfs-api-devel
    sudo yum install -y	glusterfs-devel
    sudo yum install -y xfsprogs-devel
    sudo yum install -y pam-devel
    sudo yum install -y libtasn1-devel
    sudo yum install -y openldap-devel
    sudo yum install -y libcap-devel
    sudo yum install -y libarchive-devel
    sudo yum install -y libaio-devel
    sudo yum install -y jansson-devel
    sudo yum install -y gpgme-devel
    sudo yum install -y gnutls-devel
    sudo yum install -y perl-Parse-Yapp.noarch
    sudo yum install -y systemd-devel.x86_64
}

function talloc_dep()
{
    sudo yum install -y python-devel    # for python2.x installs
    sudo yum install -y python3-devel   # for python3.x installs
}

samba_dep
talloc_dep


