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
#define KEY_AUTOLOAD		6
#define KEY_BEGIN		7
#define KEY_UNITCHECK		8
#define KEY_CORE		9
#define KEY_DESTROY		10
#define KEY_END			11
#define KEY_INIT		12
#define KEY_CHECK		13
#define KEY_abs			14
#define KEY_accept		15
#define KEY_alarm		16
#define KEY_and			17
#define KEY_atan2		18
#define KEY_bind		19
#define KEY_binmode		20
#define KEY_bless		21
#define KEY_break		22
#define KEY_caller		23
#define KEY_chdir		24
#define KEY_chmod		25
#define KEY_chomp		26
#define KEY_chop		27
#define KEY_chown		28
#define KEY_chr			29
#define KEY_chroot		30
#define KEY_close		31
#define KEY_closedir		32
#define KEY_cmp			33
#define KEY_connect		34
#define KEY_continue		35
#define KEY_cos			36
#define KEY_crypt		37
#define KEY_dbmclose		38
#define KEY_dbmopen		39
#define KEY_default		40
#define KEY_defined		41
#define KEY_delete		42
#define KEY_die			43
#define KEY_do			44
#define KEY_dump		45
#define KEY_each		46
#define KEY_else		47
#define KEY_elsif		48
#define KEY_endgrent		49
#define KEY_endhostent		50
#define KEY_endnetent		51
#define KEY_endprotoent		52
#define KEY_endpwent		53
#define KEY_endservent		54
#define KEY_eof			55
#define KEY_eq			56
#define KEY_err			57
#define KEY_eval		58
#define KEY_exec		59
#define KEY_exists		60
#define KEY_exit		61
#define KEY_exp			62
#define KEY_fcntl		63
#define KEY_fileno		64
#define KEY_flock		65
#define KEY_for			66
#define KEY_foreach		67
#define KEY_fork		68
#define KEY_ge			69
#define KEY_getc		70
#define KEY_getgrent		71
#define KEY_getgrgid		72
#define KEY_getgrnam		73
#define KEY_gethostbyaddr	74
#define KEY_gethostbyname	75
#define KEY_gethostent		76
#define KEY_getlogin		77
#define KEY_getnetbyaddr	78
#define KEY_getnetbyname	79
#define KEY_getnetent		80
#define KEY_getpeername		81
#define KEY_getpgrp		82
#define KEY_getppid		83
#define KEY_getpriority		84
#define KEY_getprotobyname	85
#define KEY_getprotobynumber	86
#define KEY_getprotoent		87
#define KEY_getpwent		88
#define KEY_getpwnam		89
#define KEY_getpwuid		90
#define KEY_getservbyname	91
#define KEY_getservbyport	92
#define KEY_getservent		93
#define KEY_getsockname		94
#define KEY_getsockopt		95
#define KEY_given		96
#define KEY_glob		97
#define KEY_gmtime		98
#define KEY_goto		99
#define KEY_grep		100
#define KEY_gt			101
#define KEY_hex			102
#define KEY_if			103
#define KEY_index		104
#define KEY_int			105
#define KEY_ioctl		106
#define KEY_join		107
#define KEY_keys		108
#define KEY_kill		109
#define KEY_last		110
#define KEY_lc			111
#define KEY_lcfirst		112
#define KEY_le			113
#define KEY_length		114
#define KEY_link		115
#define KEY_listen		116
#define KEY_local		117
#define KEY_localtime		118
#define KEY_lock		119
#define KEY_log			120
#define KEY_lstat		121
#define KEY_lt			122
#define KEY_m			123
#define KEY_map			124
#define KEY_mkdir		125
#define KEY_msgctl		126
#define KEY_msgget		127
#define KEY_msgrcv		128
#define KEY_msgsnd		129
#define KEY_my			130
#define KEY_ne			131
#define KEY_next		132
#define KEY_no			133
#define KEY_not			134
#define KEY_oct			135
#define KEY_open		136
#define KEY_opendir		137
#define KEY_or			138
#define KEY_ord			139
#define KEY_our			140
#define KEY_pack		141
#define KEY_package		142
#define KEY_pipe		143
#define KEY_pop			144
#define KEY_pos			145
#define KEY_print		146
#define KEY_printf		147
#define KEY_prototype		148
#define KEY_push		149
#define KEY_q			150
#define KEY_qq			151
#define KEY_qr			152
#define KEY_quotemeta		153
#define KEY_qw			154
#define KEY_qx			155
#define KEY_rand		156
#define KEY_read		157
#define KEY_readdir		158
#define KEY_readline		159
#define KEY_readlink		160
#define KEY_readpipe		161
#define KEY_recv		162
#define KEY_redo		163
#define KEY_ref			164
#define KEY_rename		165
#define KEY_require		166
#define KEY_reset		167
#define KEY_return		168
#define KEY_reverse		169
#define KEY_rewinddir		170
#define KEY_rindex		171
#define KEY_rmdir		172
#define KEY_s			173
#define KEY_say			174
#define KEY_scalar		175
#define KEY_seek		176
#define KEY_seekdir		177
#define KEY_select		178
#define KEY_semctl		179
#define KEY_semget		180
#define KEY_semop		181
#define KEY_send		182
#define KEY_setgrent		183
#define KEY_sethostent		184
#define KEY_setnetent		185
#define KEY_setpgrp		186
#define KEY_setpriority		187
#define KEY_setprotoent		188
#define KEY_setpwent		189
#define KEY_setservent		190
#define KEY_setsockopt		191
#define KEY_shift		192
#define KEY_shmctl		193
#define KEY_shmget		194
#define KEY_shmread		195
#define KEY_shmwrite		196
#define KEY_shutdown		197
#define KEY_sin			198
#define KEY_sleep		199
#define KEY_socket		200
#define KEY_socketpair		201
#define KEY_sort		202
#define KEY_splice		203
#define KEY_split		204
#define KEY_sprintf		205
#define KEY_sqrt		206
#define KEY_srand		207
#define KEY_stat		208
#define KEY_state		209
#define KEY_study		210
#define KEY_sub			211
#define KEY_substr		212
#define KEY_symlink		213
#define KEY_syscall		214
#define KEY_sysopen		215
#define KEY_sysread		216
#define KEY_sysseek		217
#define KEY_system		218
#define KEY_syswrite		219
#define KEY_tell		220
#define KEY_telldir		221
#define KEY_tie			222
#define KEY_tied		223
#define KEY_time		224
#define KEY_times		225
#define KEY_tr			226
#define KEY_truncate		227
#define KEY_uc			228
#define KEY_ucfirst		229
#define KEY_umask		230
#define KEY_undef		231
#define KEY_unless		232
#define KEY_unlink		233
#define KEY_unpack		234
#define KEY_unshift		235
#define KEY_untie		236
#define KEY_until		237
#define KEY_use			238
#define KEY_utime		239
#define KEY_values		240
#define KEY_vec			241
#define KEY_wait		242
#define KEY_waitpid		243
#define KEY_wantarray		244
#define KEY_warn		245
#define KEY_when		246
#define KEY_while		247
#define KEY_write		248
#define KEY_x			249
#define KEY_xor			250
#define KEY_y			251

/* ex: set ro: */