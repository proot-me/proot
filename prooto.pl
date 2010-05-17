#! /bin/env perl

use v5.10;
#use strict; #TODO: define IS_FINAL and MAX_READLINK
#use warnings;

use Cwd qw/realpath/;

my $new_root    = $ARGV[0] // '/usr/local/armedslack';
my $fake_path   = $ARGV[1] // '/bin/bash';
my $deref_final = $ARGV[2] // 1;
my $fake_cwd    = $ARGV[3] // '/tmp';

   $new_root = realpath($new_root) or die "realpath($new_root): $!";
my @fake_cwd = split('/', $fake_cwd);

sub canonicalize;

##################################################

my @canon_fake_path = canonicalize($fake_path, $deref_final, @fake_cwd);
say join '/', $new_root, @canon_fake_path;

##################################################

sub canonicalize()
{
    my ($fake_path, $deref_final, @relative_to) = @_;
    state $nb_readlink = 0;

    # Sanity checks.
    die if not defined $new_root or $new_root != m{^/};
    die if not defined $fake_path;
    $deref_final //= 1;

    # Absolutize relative $fake_path.
    my @result = $fake_path !~ m{^/} ? @relative_to : ();

    # Canonicalize recursely $fake_path into @result.
    foreach my $entry (split '/', $fake_path) {
	if ($entry eq '.' or $entry eq '') {
	    next;
	}
	if ($entry eq '..') {
	    pop @result;
	    next;
	}

	my $real_entry = join('/', $new_root, @result) . "/$entry";

	# Nothing special to do if it's not a link or if we explicitly
	# ask to not dereference $fake_path, as required by syscalls
	# like lstat(2). Obviously, this later condition does not
	# apply to intermediate path entries.
	if (not -l $real_entry or (not $deref_final and IS_FINAL($entry))) {
	    push @result, $entry;
	    next;
	}

	# It's a link, so we have to dereference *and* canonicalize to
	# ensure we are not going outside the new root.

	my $referee = readlink $real_entry;
	$nb_readlink++;

	die if not defined $referee;
	       # TODO: or $nb_readlink > MAX_READLINK;

	# Canonicalize recursively the referee in case it is/contains
	# a link. If it is not an absolute link then it is relative to
	# @result.
	@result = canonicalize($referee, 0, @result);
    }

    $nb_readlink = 0;

    return @result;
}
