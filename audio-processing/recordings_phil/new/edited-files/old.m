% set default directory for input data
testfiledir = '.';
close all;

txtfiles = dir(fullfile(testfiledir, '*/*.txt'));
nfiles = length(txtfiles);
data  = cell(nfiles);

s = struct('index',1)
current_folder = '';
folder_i = 0;

for i = 1 : nfiles
   fid = fopen( fullfile(txtfiles(i).folder , txtfiles(i).name) );
   s(i).index = i;
   s(i).filename = txtfiles(i).name;
   [s(i).directory, s(i).basename, s(i).extension] = fileparts(txtfiles(i).name);

   % make sure we got "both" of the extensions off of ".txt.txt" files
   [junk_dir s(i).basename junk_ext] = fileparts(s(i).basename);
   s(i).details = fscanf(fid, '%c');
   fclose(fid);

   fullfile(txtfiles(i).folder, append(s(i).basename,'.wav') )

   if (strcmp(current_folder, txtfiles(i).folder) == false)
        folder_i = folder_i + 1;
        current_folder = txtfiles(i).folder;
        folder_j = 1;
   else
        folder_j = folder_j + 1;
   end

   s(i).folder_i = folder_i;
   s(i).folder_j = folder_j;

   [s(i).audio s(i).audio_fs] = audioread(  fullfile(txtfiles(i).folder, append(s(i).basename,'.wav') ));

   %sound(s(i).audio, s(1).audio_fs);
   %figure(i);
   % for stereo --specgram([s(i).audio(:,1)' s(i).audio(:,2)'],1024*4,s(i).audio_fs);
   %specgram(s(i).audio(:,1),1024*4,s(i).audio_fs);
   %figure(1000+i);
   %periodogram(s(i).audio(:,1),[],'onesided',1024*64,s(i).audio_fs);
end

 
%data processing starts here
strokes={'a-pluck' 'c-pluck' 'e-pluck' 'd-pluck' 'g-pluck'};

for( si=1:length(strokes))
    mp=0;
    for x=1:nfiles
        if (strfind(s(x).basename, strokes(si)))
            mp = mp+1;
        end
    end
 
    sp=1;
    for x=1:nfiles
        if (strfind(s(x).basename, 'iPhone'))     
            disp(sprintf('skipping %s', s(x).basename));
            continue;
        end

        if (strfind(s(x).basename, strokes(si)))
            figure(si);
            subplot(mp,1,sp);
            specgram(s(x).audio(:,1),1024*4,s(x).audio_fs)
            title(s(x).basename);

            figure(si+1000);
            subplot(mp,1,sp);
            [pxx w] = periodogram(s(x).audio(:,1),[],'onesided',1024*64,s(x).audio_fs);

            if (sp == 1)
                spxx = pxx/max(pxx);
            else
                spxx = pxx/max(pxx)+spxx;
            end

            periodogram(s(x).audio(:,1),[],'onesided',1024*64,s(x).audio_fs);
            title(s(x).basename);

            sp = sp+1;
        end
    end

    spxx(si) = spxx(si) / sp;

    colors=['r','g','b','y','m'];
    sp=1;

    for x=1:nfiles
        if (strfind(s(x).basename, 'iPhone'))     
            disp(sprintf('skipping %s', s(x).basename));
            continue;
        end

        if (strfind(s(x).basename, strokes(si)))
            figure(2000+si);
            hold on

            %subplot(mp,1,sp);
            [pxx w] = periodogram(s(x).audio(:,1),[],'onesided',1024*64,s(x).audio_fs);     

            plot(10*log10(pxx),'color',colors(sp))
            title(s(x).basename);
            sp = sp+1;
            x
            hold off
        end
    end
end

%hack up
sx=[10 19 29 39 59];
td=[s(sx(1)).audio(1:150000,1) s(sx(2)).audio(1:150000,1) s(sx(3)).audio(1:150000,1) s(sx(4)).audio(1:150000,1) s(sx(5)).audio(1:150000,1)];
figure(4000);

tdm = td - mean((td(:,:)));
tdg = tdm ./ min((tdm(:,:)));
tdgm = tdg ./ max((tdg(:,:)));

figure();
plot(td);
figure();
plot(tdm);
figure();
plot(tdg);
figure();
plot(tdgm);