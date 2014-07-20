$(document).ready(function(){
	$('.ytvideo').each(function(){
		$(this).html('<div id="'+this.getAttribute('data-videoid')+'"></div><script> var tag = document.createElement(\'script\'); tag.src = "https://www.youtube.com/player_api"; var firstScriptTag = document.getElementsByTagName(\'script\')[0]; firstScriptTag.parentNode.insertBefore(tag, firstScriptTag);  var player;  function onYouTubePlayerAPIReady() { player = new YT.Player(\''+this.getAttribute('data-videoid')+'\', { height: \'auto\', width: \'auto\',  videoId: \''+this.getAttribute('data-videoid')+'\' });}</script>')
	});

	initTableAZ();
});


						//Script for preview/export only
						function initTableAZ(){
							$('div.aztable').each(function(){
								var module = $(this);
								var letters = [];
								$(this).find('table.aztable tbody tr').each(function(i){
									var row = $(this);
									var cell = row.find('td div.cell-value').first();
									var letter = cell.html().substr(0,1);
									row.addClass('aztable-'+letter);
									if(jQuery.inArray(letter, letters) < 0)
										letters.push(letter);
								});
								
								var list = module.find('ul');
								$(letters).each(function(){
									var listHTML = list.html();
									list.html(listHTML + '<li class="letter-navigation-item"><a class="letter-navigation-link" href="#" onclick="setTableAZ(\''+this+'\', this); return false;">'+this+'</a></li>');
								});
								list.find('li').each(function(i){
									var letter = $(this).html();
									if(letter.length>1)
										letter = 'all';
								});
								
							});						
						}
						
						function setTableAZ(letter, elm){
							$('.aztable tr').removeClass('is-visuallyhidden');
							$('.letter-navigation-link').removeClass('is-active');
							$(elm).addClass('is-active');
							if(letter != 'all'){
								$('.aztable tr').each(function(i){
									if(i>0){ //erste Zeile enth�lt die Headline
										if(!$(this).hasClass('aztable-'+letter)){
											$(this).addClass('is-visuallyhidden');
										}
									}
								});
							}
						}
