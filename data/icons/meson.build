icon_sizes = ['16x16', '22x22', '24x24', '32x32', '48x48', '256x256']

foreach icon_size: icon_sizes
	icon_dir = join_paths(icon_size, 'apps')

	install_data(
		join_paths(icon_dir, 'com.github.gkarsay.parlatype.png'),
		install_dir: join_paths(datadir, 'icons', 'hicolor', icon_dir),
	)
endforeach

symbolic_dir = join_paths('scalable', 'apps')

install_data(
	join_paths(symbolic_dir, 'com.github.gkarsay.parlatype-symbolic.svg'),
	install_dir: join_paths(datadir, 'icons', 'hicolor', symbolic_dir),
)
