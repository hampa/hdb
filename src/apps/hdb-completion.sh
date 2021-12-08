# hdb-get completion


_hdb-dump()
{
	local cur 

	COMPREPLY=()
	cur=${COMP_WORDS[COMP_CWORD]}

	COMPREPLY=( $( compgen -W '$( hdb -L )' -- $cur ) )

	return 0
}

_hdb()
{
	local cur 

	COMPREPLY=()
	cur=${COMP_WORDS[COMP_CWORD]}

	if [ -z "$cur"  ]; then
       COMPREPLY=( $( compgen -W '$( hdb -L )' -- $cur ) )
	else
   		COMPREPLY=( $( compgen -W '$( hdb -L )' -- $cur ) )
		parentlist=`dirname $cur | tr '/' '\\/'`
		key=`basename $cur`
		echo "$cur" | grep "/$" >/dev/null
		if [ $? -eq 0 ]; then
			parentlist=$cur
		fi
		
		#echo "parentlist $parentlist key $key"
		if [ "$parentlist" == "." ]; then	
           COMPREPLY=( $( compgen -W '$( hdb -L )' -- $cur ) )
		else
			COMPREPLY=( $( compgen -W '$( hdb -Z $parentlist ; hdb -P $parentlist | cut -d " " -f 1 )' -- $cur ) )
		fi
	fi

	return 0
}
complete -F _hdb hdb-get 
complete -F _hdb hdb
complete -F _hdb hdb-set 
complete -F _hdb hdb-delete
complete -F _hdb hdb-del
complete -F _hdb hdb-find
complete -F _hdb hdb-query
complete -F _hdb-dump hdb-dump 

