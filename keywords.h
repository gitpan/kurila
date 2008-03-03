/* -*- buffer-read-only: t -*-
 *
 *    keywords.h
 *
 *    Copyright (C) 1994, 1995, 1996, 1997, 1999, 2000, 2001, 2002, 2005,
 *    2006, 2007, by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * !!!!!!!   DO NOT EDIT THIS FILE   !!!!!!!
 *  This file is built by keywords.pl from its data.  Any changes made here
 *  will be lost!
 */
#define KEY_NULL		0
#define KEY___FILE__		1
#define KEY___LINE__		2
#define KEY___PACKAGE__		3
#define KEY___DATA__		4
#define KEY___END__		5
#define KEY_BEGIN		6
#define KEY_UNITCHECK		7
#define KEY_CORE		8
#define KEY_DESTROY		9
#define KEY_END			10
#define KEY_INIT		11
#define KEY_CHECK		12
#define KEY_abs			13
#define KEY_accept		14
#define KEY_alarm		15
#define KEY_and			16
#define KEY_atan2		17
#define KEY_bind		18
#define KEY_binmode		19
#define KEY_bless		20
#define KEY_break		21
#define KEY_caller		22
#define KEY_chdir		23
#define KEY_chmod		24
#define KEY_chomp		25
#define KEY_chop		26
#define KEY_chown		27
#define KEY_chr			28
#define KEY_chroot		29
#define KEY_close		30
#define KEY_closedir		31
#define KEY_cmp			32
#define KEY_connect		33
#define KEY_continue		34
#define KEY_cos			35
#define KEY_crypt		36
#define KEY_default		37
#define KEY_defined		38
#define KEY_delete		39
#define KEY_die			40
#define KEY_do			41
#define KEY_dump		42
#define KEY_each		43
#define KEY_else		44
#define KEY_elsif		45
#define KEY_endgrent		46
#define KEY_endhostent		47
#define KEY_endnetent		48
#define KEY_endprotoent		49
#define KEY_endpwent		50
#define KEY_endservent		51
#define KEY_eof			52
#define KEY_eq			53
#define KEY_eval		54
#define KEY_exec		55
#define KEY_exists		56
#define KEY_exit		57
#define KEY_exp			58
#define KEY_fcntl		59
#define KEY_fileno		60
#define KEY_flock		61
#define KEY_for			62
#define KEY_foreach		63
#define KEY_fork		64
#define KEY_getc		65
#define KEY_getgrent		66
#define KEY_getgrgid		67
#define KEY_getgrnam		68
#define KEY_gethostbyaddr	69
#define KEY_gethostbyname	70
#define KEY_gethostent		71
#define KEY_getlogin		72
#define KEY_getnetbyaddr	73
#define KEY_getnetbyname	74
#define KEY_getnetent		75
#define KEY_getpeername		76
#define KEY_getpgrp		77
#define KEY_getppid		78
#define KEY_getpriority		79
#define KEY_getprotobyname	80
#define KEY_getprotobynumber	81
#define KEY_getprotoent		82
#define KEY_getpwent		83
#define KEY_getpwnam		84
#define KEY_getpwuid		85
#define KEY_getservbyname	86
#define KEY_getservbyport	87
#define KEY_getservent		88
#define KEY_getsockname		89
#define KEY_getsockopt		90
#define KEY_given		91
#define KEY_glob		92
#define KEY_gmtime		93
#define KEY_goto		94
#define KEY_grep		95
#define KEY_hex			96
#define KEY_if			97
#define KEY_index		98
#define KEY_int			99
#define KEY_ioctl		100
#define KEY_join		101
#define KEY_keys		102
#define KEY_kill		103
#define KEY_last		104
#define KEY_lc			105
#define KEY_lcfirst		106
#define KEY_length		107
#define KEY_link		108
#define KEY_listen		109
#define KEY_local		110
#define KEY_localtime		111
#define KEY_lock		112
#define KEY_log			113
#define KEY_lstat		114
#define KEY_m			115
#define KEY_map			116
#define KEY_mkdir		117
#define KEY_msgctl		118
#define KEY_msgget		119
#define KEY_msgrcv		120
#define KEY_msgsnd		121
#define KEY_my			122
#define KEY_ne			123
#define KEY_next		124
#define KEY_no			125
#define KEY_not			126
#define KEY_oct			127
#define KEY_open		128
#define KEY_opendir		129
#define KEY_or			130
#define KEY_ord			131
#define KEY_our			132
#define KEY_pack		133
#define KEY_package		134
#define KEY_pipe		135
#define KEY_pop			136
#define KEY_pos			137
#define KEY_print		138
#define KEY_printf		139
#define KEY_prototype		140
#define KEY_push		141
#define KEY_q			142
#define KEY_qq			143
#define KEY_qr			144
#define KEY_quotemeta		145
#define KEY_qw			146
#define KEY_qx			147
#define KEY_rand		148
#define KEY_read		149
#define KEY_readdir		150
#define KEY_readline		151
#define KEY_readlink		152
#define KEY_readpipe		153
#define KEY_recv		154
#define KEY_redo		155
#define KEY_ref			156
#define KEY_rename		157
#define KEY_require		158
#define KEY_return		159
#define KEY_reverse		160
#define KEY_rewinddir		161
#define KEY_rindex		162
#define KEY_rmdir		163
#define KEY_s			164
#define KEY_scalar		165
#define KEY_seek		166
#define KEY_seekdir		167
#define KEY_select		168
#define KEY_semctl		169
#define KEY_semget		170
#define KEY_semop		171
#define KEY_send		172
#define KEY_setgrent		173
#define KEY_sethostent		174
#define KEY_setnetent		175
#define KEY_setpgrp		176
#define KEY_setpriority		177
#define KEY_setprotoent		178
#define KEY_setpwent		179
#define KEY_setservent		180
#define KEY_setsockopt		181
#define KEY_shift		182
#define KEY_shmctl		183
#define KEY_shmget		184
#define KEY_shmread		185
#define KEY_shmwrite		186
#define KEY_shutdown		187
#define KEY_sin			188
#define KEY_sleep		189
#define KEY_socket		190
#define KEY_socketpair		191
#define KEY_sort		192
#define KEY_splice		193
#define KEY_split		194
#define KEY_sprintf		195
#define KEY_sqrt		196
#define KEY_srand		197
#define KEY_stat		198
#define KEY_state		199
#define KEY_study		200
#define KEY_sub			201
#define KEY_substr		202
#define KEY_symlink		203
#define KEY_syscall		204
#define KEY_sysopen		205
#define KEY_sysread		206
#define KEY_sysseek		207
#define KEY_system		208
#define KEY_syswrite		209
#define KEY_tell		210
#define KEY_telldir		211
#define KEY_tie			212
#define KEY_tied		213
#define KEY_time		214
#define KEY_times		215
#define KEY_tr			216
#define KEY_truncate		217
#define KEY_uc			218
#define KEY_ucfirst		219
#define KEY_umask		220
#define KEY_undef		221
#define KEY_unless		222
#define KEY_unlink		223
#define KEY_unpack		224
#define KEY_unshift		225
#define KEY_untie		226
#define KEY_until		227
#define KEY_use			228
#define KEY_utime		229
#define KEY_values		230
#define KEY_vec			231
#define KEY_wait		232
#define KEY_waitpid		233
#define KEY_wantarray		234
#define KEY_warn		235
#define KEY_when		236
#define KEY_while		237
#define KEY_write		238
#define KEY_x			239
#define KEY_xor			240
#define KEY_y			241

/* ex: set ro: */
