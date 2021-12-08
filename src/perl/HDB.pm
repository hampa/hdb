#!/usr/bin/perl -w
#########################################################################
# HDB, Copyright (C) 2006 Hampus Soderstrom
# All rights reserved.  Email: hampus@sxp.se
#
# This library is free software; you can redistribute it and/or         
# modify it under the terms of The GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of the 
# License, or (at your option) any later version. The text of the GNU
# Lesser General Public License is included with this library in the
# file LICENSE.TXT.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the files
# LICENSE.TXT for more details.
#########################################################################
package HDB;
use IO::Socket;
use URI::Escape;
use strict;

my ($hdb_error) = 'ENONE';

sub new 
{
    my ($class, %h) = @_;
    my ($host, $port);
    my $self = {};    
    bless $self, $class;

    $self->set_error;
    $self->{'quit'} = 0;

    if (defined $h{'Host'})
    {
	($host, $port) = split(/:/, $h{'Host'});
    }
    $host = "127.0.0.1" unless $host;
    $port = "12429" unless $port;
    $self->{'host'} = $host;
    $self->{'port'} = $port;
    $self->{'doAutoflush'} = 0;

    $self->connect() || return undef;
    $self;
}

sub autoflush
{
    my ($self, $doAutoflush) = @_;
    if ($doAutoflush == 0)
    {
	$self->{'doAutoflush'} = 0;
    }
    else
    {
	$self->{'doAutoflush'} = 1;	
    }
}

sub connect
{
    my ($self) = @_;
    $self->{'sock'} = new IO::Socket::INET( PeerAddr => $self->{'host'},
					    PeerPort => $self->{'port'},
					    Type => SOCK_STREAM,
					    ReuseAddr => 1,
					    Proto => 'tcp',
					    TimeOut => 3
					    ) or return $self->set_error('ESOCKETFAIL');
    $self->{'sock'}->getline();
    if (defined $self->{'root'})
    {
	$self->ROOT($self->{'root'}, 1);
    }
    else
    {
	$self->{'root'} = $self->ROOT();
    }
}

sub DESTROY
{
     my ($self) = @_;
     $self->QUIT;
}

sub QUIT
{
     my ($self) = @_;
     if ($self->{'quit'} == 0)
     { 
	 $self->execute("QUIT");
	 $self->{'sock'}->shutdown(2);
	 $self->{'quit'} = 1;
     }
}

sub execute
{
    my ($self, $command, $retries) = @_;
    $retries = 0 unless $retries;
    return 0 if ($retries > 3);
    $self->FLUSH() if ($self->{'doAutoflush'} == 1);
    if (defined $self->{'sock'})
    {
	$self->{'sock'}->write("$command\n");
    }
    else
    {
	# Server dead ?!?
	# Reconnect and execute command again
	sleep 1;
	$self->connect || return 0;
	return $self->execute($command, ++$retries);
    }
    @{$self->{'result'}} = ();
    my $line = $self->{'sock'}->getline() || "#DEAD";
    chomp $line;
    while ($line !~ /^\#(.*)/)
    {
	push @{$self->{'result'}}, $line;
	$line = $self->{'sock'}->getline() || "#DEAD";
	chomp $line;
    }
    $line =~ /\#(.*)/;
    $self->{'status'} = $1;
    if ($1 eq "OK")
    {
	return 1;
    }
    elsif ($1 eq "DEAD")
    {
	# Server dead ?!?
	# Reconnect and execute command again
	sleep 1;
	$self->connect ||return 0;
	return $self->execute($command, ++$retries);
    }
    else
    {
	return 0;
    }
}

sub GET
{
    my ($self, $list, $val) = @_; 
    #$self->execute("GET $list $val");
    $self->execute("0 $list $val");
    return ${$self->{'result'}}[0];
}

sub UPDATE
{
    my ($self, $list, $key, $val) = @_;
    $val =~ s/\\/\\\\/g;
    $val =~ s/\"/\\\"/g;
    return $self->execute("UPDATE $list $key " . uri_escape($val));
}

sub SET
{
    my ($self, $list, $key, $val) = @_;
    $val =~ s/\\/\\\\/g;
    $val =~ s/\"/\\\"/g;
    return $self->execute("1 $list $key " . uri_escape($val));
}

sub SYNC 
{
    my ($self) = @_;
    #return $self->execute("SYNC");
    return $self->execute("23");
}

sub LIST
{
    my ($self) = @_;
    #$self->execute("LIST");
    $self->execute("21");
    return @{$self->{'result'}};
}

sub SUBLIST
{
    my ($self, $list) = @_;
    #$self->execute("SUBLIST $list");
    $self->execute("30 $list");
    return @{$self->{'result'}};
}

sub PRINT
{
    my ($self, $list) = @_;
    my %h = ();
    #$self->execute("PRINT $list");
    $self->execute("2 $list");
    for (@{$self->{'result'}})
    {
	my ($key, $val) = split;
	$h{$key} = $val;
    }
    return %h;
}

sub ROOT
{
    my ($self, $root, $force) = @_;
    if (!defined($root))
    {
	#$self->execute("ROOT");
	$self->execute("33");
	return ${$self->{'result'}}[0];
    }
    elsif ($root ne $self->{'root'} || $force == 1)
    {
	$self->{'root'} = $root;
	#return $self->execute("ROOT $root");
	return $self->execute("33 $root");
    }
    else
    {
	return $self->{'root'};
    }
}

sub DUMP
{
    my ($self, $list) = @_;
    #$self->execute("DUMP $list");
    $self->execute("3 $list");
    return @{$self->{'result'}};
}

sub set_error 
{
    my ($self, $error) = @_;
    $hdb_error = $self->{'error'} = defined $error ? $error : 'ENONE';
    return undef;
}

sub get_error 
{
    my ($self) = @_;
    $self->{'error'};
}

sub strerror 
{
    my ($self, $error) = @_;

    my %errors = (
		  'ENONE',      'none',
		  'ESELECTFAIL','select creation failed',
		  'ETIMEOUT',   'timed out waiting for packet',
		  'ESOCKETFAIL','socket creation failed',
		  'ENOHOST',    'no host specified',
		  'EBADAUTH',   'bad response authenticator',
		  'ESENDFAIL',	'send failed',
		  'ERECVFAIL',	'receive failed'
		  );

    return $errors{$hdb_error} unless ref($self);
    $errors{defined $error ? $error : $self->{'error'}};
}

1;
